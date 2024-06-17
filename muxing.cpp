/**************************************************************************
 * File: muxing.c
 * Description: This program reads a H.264 file, then muxes it into a .MP4 
 				file. Using FFmpeg libraries, it opens the input file, reads 
 				the video stream, and writes it to the output file.
**************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" {
	#include <libavutil/opt.h>
	#include <libavutil/mathematics.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libswresample/swresample.h>
}

int main(int argc, char **argv) {
    const char *input_filename = "test.264";
	const char *output_filename = "test.mp4";

	AVPacket packet;
	AVDictionary *opts = NULL;
	AVFormatContext *output_format_context = NULL, *input_format_context = NULL;
	AVStream *video_stream = NULL;
	int ret;
	int frame_index = 0;

    av_register_all();

	do {
		if((ret = avformat_open_input(&input_format_context, input_filename, NULL, NULL)) < 0) {
			fprintf(stderr, "Could not open input file");
			break;
		}

		if((ret = avformat_find_stream_info(input_format_context, 0)) < 0) {
			fprintf(stderr, "Failed to retrieve input stream information");
			break;
		}

		avformat_alloc_output_context2(&output_format_context, NULL, NULL, output_filename);

		video_stream = avformat_new_stream(output_format_context, NULL);
		if(video_stream == NULL) {
			fprintf(stderr, "Failed to create video stream\n");
			break;
		}

		avcodec_parameters_copy(video_stream->codecpar, input_format_context->streams[0]->codecpar);
		av_dump_format(output_format_context, 0, output_filename, 1);

		avio_open(&output_format_context->pb, output_filename, AVIO_FLAG_WRITE);

		av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
		ret = avformat_write_header(output_format_context, &opts);
		if(ret < 0) {
			fprintf(stderr, "Error occurred when opening output file: %s\n");
			break;
		}

		while (1) {
			ret = av_read_frame(input_format_context, &packet);
			if(ret < 0)
				break;

			packet.stream_index = 0;
			AVRational time_base1 = input_format_context->streams[0]->time_base;
			int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(input_format_context->streams[0]->r_frame_rate);
			packet.pts = (double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			packet.dts = packet.pts;
			packet.duration = (double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			frame_index++;
			packet.pts = av_rescale_q_rnd(packet.pts, input_format_context->streams[0]->time_base, output_format_context->streams[0]->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
			packet.dts = av_rescale_q_rnd(packet.dts, input_format_context->streams[0]->time_base, output_format_context->streams[0]->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
			packet.duration = av_rescale_q(packet.duration, input_format_context->streams[0]->time_base, output_format_context->streams[0]->time_base);
			packet.pos = -1;

			ret = av_interleaved_write_frame(output_format_context, &packet);

			if(ret < 0) {
				fprintf(stderr, "Error muxing packet\n");
				break;
			}

			av_packet_unref(&packet);
		}

		av_write_trailer(output_format_context);

	} while(0);

	avformat_free_context(output_format_context);
	avformat_free_context(input_format_context);
	return 0;
}