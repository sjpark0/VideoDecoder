
#include <iostream>
#include <string>
#include <vector>
#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include "VideoDecoder.h"

int FindIFrame(int argc, char* argv[]) {

    char* inputFilename = argv[1];
    
    AVFormatContext* format_ctx = nullptr;
    if (avformat_open_input(&format_ctx, inputFilename, nullptr, nullptr) < 0) {
        std::cerr << "Could not open the file." << std::endl;
        return 1;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        avformat_close_input(&format_ctx);
        return 1;
    }

    int video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        std::cerr << "Could not find a video stream." << std::endl;
        avformat_close_input(&format_ctx);
        return 1;
    }

    AVPacket* packet = av_packet_alloc();
	int frame = 0;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index && (packet->flags & AV_PKT_FLAG_KEY)) {
            std::cout << "I-Frame found at position " << packet->pos << ", PTS: " << packet->pts << ",frame : " << frame << std::endl;
        }
		frame++;
        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    avformat_close_input(&format_ctx);
    avformat_network_deinit();
    return 0;

}
