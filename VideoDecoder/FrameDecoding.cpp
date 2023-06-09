#include <iostream>
#include <fstream>
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


void save_frame_jpg(AVFrame *frame, int width, int height, int frame_number, const char *outputFolder) {
    
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec) {
        std::cerr << "Codec not found" << std::endl;
        exit(1);
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Could not allocate codec context" << std::endl;
        exit(1);
    }
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = codec->pix_fmts[0];

    codec_ctx->time_base.num = 1;
    codec_ctx->time_base.den = 25;
    
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        exit(1);
    }
	
    AVPacket *pkt = av_packet_alloc();
	//AVPacket *pkt = new AVPacket();
	//pkt.data = nullptr;
	//pkt.size = 0;

    int ret;
    int got_output;
	if ((ret = avcodec_send_frame(codec_ctx, frame)) < 0) {
        char err_msg[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(err_msg, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Error sending a frame for encoding: " << err_msg << std::endl;
        exit(1);
    }
	
    if ((ret = avcodec_receive_packet(codec_ctx, pkt)) < 0) {	
        std::cerr << "Error during encoding" << std::endl;
        exit(1);
    }
	
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.jpg", outputFolder, frame_number);
	
    std::ofstream jpg_file(filename, std::ios::binary);
    jpg_file.write(reinterpret_cast<char *>(pkt->data), pkt->size);
    jpg_file.close();

    av_packet_unref(pkt);
    avcodec_free_context(&codec_ctx);
	av_packet_free(&pkt);
	//delete pkt;

}
void save_frame_png(AVFrame* frame, int width, int height, int frame_number, const char *outputFolder) {
    av_log_set_level(AV_LOG_DEBUG);
    
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        std::cerr << "Codec not found" << std::endl;
        exit(1);
    }
    
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Could not allocate codec context" << std::endl;
        exit(1);
    }
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    codec_ctx->time_base.num = 1;
    codec_ctx->time_base.den = 25;
    
    
    
    
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        exit(1);
    }
    
    AVFrame *rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        fprintf(stderr, "Could not allocate frame\n");
        exit(1);
    }

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
    uint8_t *rgb_frame_buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
    if (!rgb_frame_buffer) {
        fprintf(stderr, "Could not allocate frame buffer\n");
        exit(1);
    }

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_frame_buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

    rgb_frame->width = codec_ctx->width;
    rgb_frame->height = codec_ctx->height;
    rgb_frame->format = codec_ctx->pix_fmt;
    
    struct SwsContext *sws_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        exit(1);
    }

    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);

    
	//AVPacket *pkt = new AVPacket();// av_packet_alloc();
	AVPacket *pkt = av_packet_alloc();
    
    int ret;
    int got_output;
    if ((ret = avcodec_send_frame(codec_ctx, rgb_frame)) < 0) {
        char err_msg[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(err_msg, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Error sending a frame for encoding: " << err_msg << std::endl;
        exit(1);
    }
    
    if ((ret = avcodec_receive_packet(codec_ctx, pkt)) < 0) {
        std::cerr << "Error during encoding" << std::endl;
        exit(1);
    }
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.png", outputFolder, frame_number);
    std::ofstream jpg_file(filename, std::ios::binary);
    jpg_file.write(reinterpret_cast<char *>(pkt->data), pkt->size);
    jpg_file.close();
    
    av_packet_unref(pkt);
    avcodec_free_context(&codec_ctx);
    avcodec_close(codec_ctx);
    av_free(codec_ctx);
    av_frame_free(&rgb_frame);
    av_free(rgb_frame_buffer);
    sws_freeContext(sws_ctx);
	av_packet_free(&pkt);
	//delete pkt;
}

int NaiveDecoding(int argc, char* argv[]) {
    const char* input_filename = argv[1];
    const char* output_folder = argv[3];
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

    AVPacket *packet = av_packet_alloc();
    int frame_number = atoi(argv[2]);
    	
    int current_frame_number = 0;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            int response = avcodec_send_packet(codec_ctx, packet);
			if (response < 0) {
                throw std::runtime_error("Error sending packet for decoding");
            }

            while (response >= 0) {
                response = avcodec_receive_frame(codec_ctx, frame);
				//printf("%d\n", frame->best_effort_timestamp);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    break;
                }
                else if (response < 0) {
                    throw std::runtime_error("Error receiving frame from codec");
                }

                if (current_frame_number == frame_number) {
                    
                    save_frame_png(frame, codec_ctx->width, codec_ctx->height, frame_number, output_folder);
                    std::cout << "Saved frame " << frame_number << " as an image file." << std::endl;

                    current_frame_number++;
                    break;
                }
                else {
                    current_frame_number++;
                }
            }
        }
        av_packet_unref(packet);
        if (current_frame_number > frame_number) {
            break;
        }
    }
	
	av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}



int FastDecoding(int argc, char* argv[]) {
    const char* input_filename = argv[1];
    const char* output_folder = argv[3];
    //av_register_all();

	AVFormatContext *format_ctx = avformat_alloc_context();
	if (avformat_open_input(&format_ctx, input_filename, nullptr, nullptr) != 0) {
		std::cerr << "Failed to open input file." << std::endl;
		return -1;
	}

	// 스트림 정보 찾기
	if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
		std::cerr << "Failed to find stream information." << std::endl;
		return -1;
	}

	// 비디오 스트림 찾기
	int video_stream_index = -1;
	AVCodecParameters *codec_params = nullptr;
	for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
		if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			codec_params = format_ctx->streams[i]->codecpar;
			break;
		}
	}

	if (video_stream_index == -1) {
		std::cerr << "Failed to find a video stream." << std::endl;
		return -1;
	}

	AVStream *video_stream = format_ctx->streams[video_stream_index];


    const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        throw std::runtime_error("Cannot find codec");
		return -1;
    }
	    
	// 코덱 컨텍스트 설정
	AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
		std::cerr << "Failed to set codec context." << std::endl;
		return -1;
	}


	// 디코더 열기
	if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
		std::cerr << "Failed to open decoder." << std::endl;
		return -1;
	}
    
    //int user_input_frame_number = atoi(argv[2]);
	//int64_t target_pts = user_input_frame_number * video_stream->time_base.den / video_stream->time_base.num / 30;
	
	int user_input_frame_number = atoi(argv[2]);
	// 사용자가 입력한 프레임의 AVIndexEntry를 획득
	const AVIndexEntry* entry = avformat_index_get_entry(video_stream, user_input_frame_number);
	// 사용자가 입력한 프레임번호에 해당하는 timestamp획득
	int64_t target_pts = entry->timestamp;

	// 가장가까운 이전 i-프레임을 찾습니다.
	int iframe_idx = av_index_search_timestamp(video_stream, target_pts, AVSEEK_FLAG_BACKWARD);
	const AVIndexEntry* entry1 = avformat_index_get_entry(video_stream, iframe_idx);
	//const AVIndexEntry* entry1 = avformat_index_get_entry_from_timestamp(video_stream, target_pts, AVSEEK_FLAG_BACKWARD);

	// 가장 가까운 I-프레임으로 이동
	if (av_seek_frame(format_ctx, video_stream_index, entry1->timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
		std::cerr << "Failed to seek to the nearest I-frame." << std::endl;
		return -1;
	}

    AVPacket *packet = av_packet_alloc();;
    AVFrame *frame = av_frame_alloc();
		
	int decoded_frame_count = iframe_idx;
	bool frame_found = false;
	while (!frame_found && av_read_frame(format_ctx, packet) >= 0) {
		if (packet->stream_index == video_stream_index) {
			if (avcodec_send_packet(codec_ctx, packet) == 0) {
				while (avcodec_receive_frame(codec_ctx, frame) == 0) {
					decoded_frame_count++;
					printf("%d, %d\n", frame->best_effort_timestamp, target_pts);
					if (frame->best_effort_timestamp >= target_pts) {
						frame_found = true;
						break;
					}
				}
			}
		}
		av_packet_unref(packet);


		if (frame_found) {
			
			printf("%d, %d\n", frame->width, frame->height);
			//save_frame(rgb_frame, codec_ctx->width, codec_ctx->height, 1, output_folder);
            save_frame_png(frame, codec_ctx->width, codec_ctx->height, decoded_frame_count, output_folder);
            save_frame_jpg(frame, codec_ctx->width, codec_ctx->height, decoded_frame_count, output_folder);
			std::cout << "Saved frame " << decoded_frame_count << " as an image file." << std::endl;

			break;
		}
	}
	

	// 메모리 해제 및 종료
	av_packet_free(&packet);
	av_frame_free(&frame);
	avcodec_close(codec_ctx);
	avcodec_free_context(&codec_ctx);
	avformat_close_input(&format_ctx);
	avformat_free_context(format_ctx);

	return 0;
}


int FastDecoding2(int argc, char* argv[]) {
	const char* input_filename = argv[1];
	const char* output_folder = argv[3];
	
	AVFormatContext *format_ctx = avformat_alloc_context();
	if (avformat_open_input(&format_ctx, input_filename, nullptr, nullptr) != 0) {
		std::cerr << "Failed to open input file." << std::endl;
		return -1;
	}

	// 스트림 정보 찾기
	if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
		std::cerr << "Failed to find stream information." << std::endl;
		return -1;
	}

	// 비디오 스트림 찾기
	int video_stream_index = -1;
	AVCodecParameters *codec_params = nullptr;
	for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
		if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			codec_params = format_ctx->streams[i]->codecpar;
			break;
		}
	}

	if (video_stream_index == -1) {
		std::cerr << "Failed to find a video stream." << std::endl;
		return -1;
	}

	AVStream *video_stream = format_ctx->streams[video_stream_index];
	
	const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
	if (!codec) {
		throw std::runtime_error("Cannot find codec");
		return -1;
	}

	// 코덱 컨텍스트 설정
	AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
		std::cerr << "Failed to set codec context." << std::endl;
		return -1;
	}


	// 디코더 열기
	if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
		std::cerr << "Failed to open decoder." << std::endl;
		return -1;
	}

	int user_input_frame_number = atoi(argv[2]);
	// 사용자가 입력한 프레임의 AVIndexEntry를 획득
	const AVIndexEntry* entry = avformat_index_get_entry(video_stream, user_input_frame_number);	
	// 사용자가 입력한 프레임번호에 해당하는 timestamp획득
	int64_t target_pts = entry->timestamp;

	// 가장가까운 이전 i-프레임을 찾습니다.
	int iframe_idx = av_index_search_timestamp(video_stream, target_pts, AVSEEK_FLAG_BACKWARD);
	const AVIndexEntry* entry1 = avformat_index_get_entry(video_stream, iframe_idx);
	//const AVIndexEntry* entry1 = avformat_index_get_entry_from_timestamp(video_stream, target_pts, AVSEEK_FLAG_BACKWARD);
		
	// 가장 가까운 I-프레임으로 이동
	if (av_seek_frame(format_ctx, video_stream_index, entry1->timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
		std::cerr << "Failed to seek to the nearest I-frame." << std::endl;
		return -1;
	}

	AVPacket *packet = av_packet_alloc();;
	AVFrame *frame = av_frame_alloc();

	int decoded_frame_count = iframe_idx;
	bool frame_found = false;
	while (av_read_frame(format_ctx, packet) >= 0) {
		if (packet->stream_index == video_stream_index) {
			if (avcodec_send_packet(codec_ctx, packet) == 0) {
				if (avcodec_receive_frame(codec_ctx, frame) == 0) {
					if (decoded_frame_count == user_input_frame_number) {
						printf("found!!\n");
						save_frame_png(frame, codec_ctx->width, codec_ctx->height, decoded_frame_count, output_folder);
						save_frame_jpg(frame, codec_ctx->width, codec_ctx->height, decoded_frame_count, output_folder);
						std::cout << frame->best_effort_timestamp << "," << target_pts << std::endl;
						std::cout << "Saved frame " << decoded_frame_count << " as an image file." << std::endl;
						av_packet_unref(packet);
						break;
					}
				}
			}
		}
		
		printf("%dth frame\n", decoded_frame_count);
		decoded_frame_count++;
		av_packet_unref(packet);		
	}


	// 메모리 해제 및 종료
	av_packet_free(&packet);
	av_frame_free(&frame);
	avcodec_close(codec_ctx);
	avcodec_free_context(&codec_ctx);
	avformat_close_input(&format_ctx);
	avformat_free_context(format_ctx);

	return 0;
}
