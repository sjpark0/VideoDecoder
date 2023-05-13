#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include "VideoDecoder.h"


void save_frame_png(AVFrame* frame, int width, int height, int frame_number) {
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    //AVOutputFormat* output_fmt = nullptr;
    AVStream* stream = nullptr;
    //AVCodec* encoder = nullptr;

    char filename[32];
    snprintf(filename, sizeof(filename), "frame_%04d.png", frame_number);

    avformat_alloc_output_context2(&format_ctx, nullptr, nullptr, filename);
    if (!format_ctx) {
        throw std::runtime_error("Cannot create output context");
    }

    const AVOutputFormat *output_fmt = format_ctx->oformat;
    const AVCodec *encoder = avcodec_find_encoder(output_fmt->video_codec);
    if (!encoder) {
        throw std::runtime_error("Cannot find PNG encoder");
    }

    stream = avformat_new_stream(format_ctx, encoder);
    if (!stream) {
        throw std::runtime_error("Cannot create output stream");
    }

    codec_ctx = avcodec_alloc_context3(encoder);
    if (!codec_ctx) {
        throw std::runtime_error("Cannot allocate codec context");
    }

    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    //codec_ctx->time_base = (AVRational){ 1, 1 };
    codec_ctx->time_base.den = 1;
    codec_ctx->time_base.num = 1;
    if (avcodec_open2(codec_ctx, encoder, nullptr) < 0) {
        throw std::runtime_error("Cannot open PNG encoder");
    }

    if (avcodec_parameters_from_context(stream->codecpar, codec_ctx) < 0) {
        throw std::runtime_error("Cannot copy codec parameters");
    }

    if (!(output_fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("Cannot open output file");
        }
    }

    if (avformat_write_header(format_ctx, nullptr) < 0) {
        throw std::runtime_error("Error writing PNG header");
    }

    AVPacket pkt;
    if (avcodec_send_frame(codec_ctx, frame) < 0) {
        throw std::runtime_error("Error sending frame to PNG encoder");
    }

    int ret = avcodec_receive_packet(codec_ctx, &pkt);
    if (ret < 0 && ret != AVERROR_EOF) {
        throw std::runtime_error("Error receiving packet from PNG encoder");
    }

    if (ret >= 0) {
        av_write_frame(format_ctx, &pkt);
        av_packet_unref(&pkt);
    }

    av_write_trailer(format_ctx);

    if (codec_ctx) {
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
    }

    if (format_ctx && !(output_fmt->flags & AVFMT_NOFILE)) {
        avio_closep(&format_ctx->pb);
    }

    avformat_free_context(format_ctx);
}

void save_frame(AVFrame* frame, int width, int height, int frame_number) {
    FILE* file;
    char filename[32];
    snprintf(filename, sizeof(filename), "frame-%04d.ppm", frame_number);

    file = fopen(filename, "wb");
    if (!file) {
        throw std::runtime_error("Cannot open output file");
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);

    for (int y = 0; y < height; y++) {
        fwrite(frame->data[0] + y * frame->linesize[0], 1, width * 3, file);
    }

    fclose(file);
}

int NaiveDecoding(int argc, char* argv[]) {
    const char* input_filename = argv[1];

    //av_register_all();

    AVFormatContext* format_ctx = nullptr;
    if (avformat_open_input(&format_ctx, input_filename, nullptr, nullptr) != 0) {
        throw std::runtime_error("Cannot open input file");
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        throw std::runtime_error("Cannot find stream information");
    }

    int video_stream_index = -1;
    AVCodecParameters* codec_params = nullptr;
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            codec_params = format_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index == -1) {
        throw std::runtime_error("Cannot find video stream");
    }

    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        throw std::runtime_error("Cannot find codec");
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw std::runtime_error("Cannot allocate codec context");
    }

    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        throw std::runtime_error("Cannot copy codec parameters to codec context");
    }

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        throw std::runtime_error("Cannot open codec");
    }


    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("Cannot allocate frame");
    }

    AVPacket packet;
    int frame_number = atoi(argv[2]);
    
    int current_frame_number = 0;
    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            int response = avcodec_send_packet(codec_ctx, &packet);
            if (response < 0) {
                throw std::runtime_error("Error sending packet for decoding");
            }

            while (response >= 0) {
                response = avcodec_receive_frame(codec_ctx, frame);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    break;
                }
                else if (response < 0) {
                    throw std::runtime_error("Error receiving frame from codec");
                }

                if (current_frame_number == frame_number) {
                    AVFrame* frame_rgb = av_frame_alloc();
                    if (!frame_rgb) {
                        throw std::runtime_error("Cannot allocate RGB frame");
                    }

                    uint8_t* buffer = nullptr;
                    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
                    buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));

                    av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

                    SwsContext* sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, frame_rgb->data, frame_rgb->linesize);

                    save_frame(frame_rgb, codec_ctx->width, codec_ctx->height, frame_number);
                    std::cout << "Saved frame " << frame_number << " as an image file." << std::endl;

                    av_frame_free(&frame_rgb);
                    av_free(buffer);
                    sws_freeContext(sws_ctx);
                    current_frame_number++;
                    break;
                }
                else {
                    current_frame_number++;
                }
            }
        }
        av_packet_unref(&packet);
        if (current_frame_number > frame_number) {
            break;
        }
    }

    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}



int FastDecoding(int argc, char* argv[]) {
    const char* input_filename = argv[1];

    //av_register_all();

    AVFormatContext* format_ctx = nullptr;
    if (avformat_open_input(&format_ctx, input_filename, nullptr, nullptr) != 0) {
        throw std::runtime_error("Cannot open input file");
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        throw std::runtime_error("Cannot find stream information");
    }

    int video_stream_index = -1;
    AVCodecParameters* codec_params = nullptr;
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            codec_params = format_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_stream_index == -1) {
        throw std::runtime_error("Cannot find video stream");
    }

    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        throw std::runtime_error("Cannot find codec");
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw std::runtime_error("Cannot allocate codec context");
    }

    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        throw std::runtime_error("Cannot copy codec parameters to codec context");
    }

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        throw std::runtime_error("Cannot open codec");
    }
    AVStream* video_stream = format_ctx->streams[video_stream_index];
    
    int user_input_frame_number = atoi(argv[2]);

    int64_t target_pts = user_input_frame_number * video_stream->time_base.den / (video_stream->time_base.num * 30);
    
    // i-프레임을 찾습니다.
    int64_t iframe_pts = av_index_search_timestamp(video_stream, target_pts, AVSEEK_FLAG_BACKWARD);
    printf("T : %d, %d\n", video_stream->time_base.den, video_stream->time_base.num);
    // 탐색을 위한 위치를 설정합니다.
    AVRational av_time_base_q = { 1, AV_TIME_BASE };
    int64_t seek_target = av_rescale_q(iframe_pts, video_stream->time_base, av_time_base_q);
    av_seek_frame(format_ctx, video_stream_index, seek_target, AVSEEK_FLAG_BACKWARD);

    printf("%d\n", seek_target);

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        throw std::runtime_error("Cannot allocate frame");
    }

    int current_frame_number = 0;
    AVPacket packet;
    int decoded_frame_number = 0;

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == video_stream_index) {
            avcodec_send_packet(codec_ctx, &packet);

            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                printf("%d, %d\n", frame->best_effort_timestamp, target_pts);
                // 원하는 프레임을 찾으면 저장하고 루프를 종료합니다.
                if (frame->best_effort_timestamp >= target_pts) {
                    save_frame(frame, codec_ctx->width, codec_ctx->height, user_input_frame_number);
                    break;
                }
            }
        }
        av_packet_unref(&packet);

        // 원하는 프레임을 이미 찾았으면 루프를 종료합니다.
        if (frame->best_effort_timestamp >= target_pts) {
            break;
        }
    }

    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}