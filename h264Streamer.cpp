/**************************************************************************
 * File: h264Streamer.c
 * Description: This program reads a H.264 file and streams it to a window.
                Using FFmpeg libraries, it opens the input file, reads the
                video stream, and displays it in a window.
**************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <SDL2/SDL.h>
}

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;     // Format context
    int videoStream;                        // Video stream index
    AVCodecContext *pCodecCtx = NULL;       // Codec context
    AVCodec *pCodec = NULL;                 // Codec
    AVFrame *pFrame = NULL;                 // Frame
    AVFrame *pFrameRGB = NULL;              // Frame in RGB format
    AVPacket packet;                        // Packet
    int frameFinished;                      // Frame finished flag
    struct SwsContext *sws_ctx = NULL;      // Sws context

    if (argc < 2) {
        printf("Usage: ./player.out <file> <play speed>\n");
        return -1;
    }

    // Register all formats and codecs
    av_register_all();

    // Open video file
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1; // Couldn't open file

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find the first video stream
    videoStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
        
    if (videoStream == -1)
        return -1; // Didn't find a video stream

    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1; // Could not open codec

    // Allocate video frame
    pFrame = av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameRGB = av_frame_alloc();
    if (pFrameRGB == NULL)
        return -1;

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);

    // Initialize SWS context for software scaling
    sws_ctx = sws_getContext(
        pCodecCtx->width,                     
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);

    // All of above is for identifying video stream and decoding video frames

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *screen = SDL_CreateWindow(
        argv[1],                    // title of the window
        SDL_WINDOWPOS_UNDEFINED,    // x position of the window
        SDL_WINDOWPOS_UNDEFINED,    // y position of the window
        pCodecCtx->width,           // width of the window
        pCodecCtx->height,          // height of the window
        0                           // flags - 0 for no flags, 1 for fullscreen, 2...(see documentation for more flags)
    );

    if (!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }

    // set up YUV->RGB conversion
    SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        pCodecCtx->width,
        pCodecCtx->height
    );

    SDL_Event event;

    // Read frames and save first five frames to disk
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if (frameFinished) {
                // Convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);

                SDL_UpdateTexture(texture, NULL, pFrameRGB->data[0], pFrameRGB->linesize[0]);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                if (argc == 3) {
                    SDL_Delay(40 / atoi(argv[2]));
                } else {
                    SDL_Delay(40);
                }
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_packet_unref(&packet);

        // Handle SDL events
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                SDL_DestroyTexture(texture);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(screen);
                SDL_Quit();
                av_free(buffer);
                av_frame_free(&pFrameRGB);
                av_frame_free(&pFrame);
                avcodec_close(pCodecCtx);
                avformat_close_input(&pFormatCtx);
                return 0;
                break;
            default:
                break;
        }
    }

    // Free the RGB image
    av_free(buffer);
    av_frame_free(&pFrameRGB);

    // Free the YUV frame
    av_frame_free(&pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}