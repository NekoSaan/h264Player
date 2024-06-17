/**************************************************************************
 * File: h264Streamer.c
 * Description: This program reads a H.264 file and streams it to a window.
                Using FFmpeg libraries, it opens the input file, reads the
                video stream, and displays it in a window.
**************************************************************************/
#include <stdexcept>
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
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
        class CannotOpenFile : public std::runtime_error {
            public:
                CannotOpenFile() : std::runtime_error("Couldn't open file") {}
        };

        throw CannotOpenFile();
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        class CannotFindStreamInfo : public std::runtime_error {
            public:
                CannotFindStreamInfo() : std::runtime_error("Couldn't find stream information") {}
        };

        throw CannotFindStreamInfo();
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find the first stream of video
    videoStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
        
    if (videoStream == -1) {
        class NoVideoStream : public std::runtime_error {
            public:
                NoVideoStream() : std::runtime_error("Didn't find a video stream") {}
        };

        throw NoVideoStream();
    }

    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream. If not available, throw an exception
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        class CodecNotFound : public std::runtime_error {
            public:
                CodecNotFound() : std::runtime_error("Codec not found") {}
        };

        throw CodecNotFound();
    }

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        class CannotOpenCodec : public std::runtime_error {
            public:
                CannotOpenCodec() : std::runtime_error("Couldn't open codec") {}
        };

        throw CannotOpenCodec();
    }

    // Allocate video frame
    pFrame = av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameRGB = av_frame_alloc();
    if (pFrameRGB == NULL) {
        class CannotAllocateFrame : public std::runtime_error {
            public:
                CannotAllocateFrame() : std::runtime_error("Couldn't allocate frame") {}
        };

        throw CannotAllocateFrame();
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);

    // Initialize SWS context for software scaling
    sws_ctx = sws_getContext(
        pCodecCtx->width,       // source width
        pCodecCtx->height,      // source height
        pCodecCtx->pix_fmt,     // source format
        pCodecCtx->width,       // destination width
        pCodecCtx->height,      // destination height
        AV_PIX_FMT_RGB24,       // destination format
        SWS_BILINEAR,           // flags, SWS_BILINEAR for better quality but slower, more flags in documentation.
        NULL,                   // source filter, NULL for default.
        NULL,                   // destination filter, NULL for default.
        NULL                    // filtering parameter, NULL for default.
    );

    // All of above is for identifying video stream and decoding video frames

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        class SDLInitError : public std::runtime_error {
            public:
                SDLInitError() : std::runtime_error("Could not initialize SDL") {}
        };

        throw SDLInitError();
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
        class SDLCreateWindowError : public std::runtime_error {
            public:
                SDLCreateWindowError() : std::runtime_error("SDL_CreateWindow failed") {}
        };

        throw SDLCreateWindowError();
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

    int64_t frame_time = 0;
    int64_t frame_increment = av_rescale_q(1, AV_TIME_BASE_Q, pFormatCtx->streams[videoStream]->time_base);

    // Read frames and save first five frames to disk
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if (frameFinished) {
                // Convert the image from its native format to RGB
                sws_scale(
                    sws_ctx, (uint8_t const *const *)pFrame->data,
                    pFrame->linesize, 0, pCodecCtx->height,
                    pFrameRGB->data, pFrameRGB->linesize
                );

                SDL_UpdateTexture(texture, NULL, pFrameRGB->data[0], pFrameRGB->linesize[0]);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                // Delay, if necessary, to get 25 frames per second
                if (argc == 3) {
                    SDL_Delay(40 / atoi(argv[2]));
                } else {
                    SDL_Delay(40);
                }
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_packet_unref(&packet);

        bool is_paused = false;

        // Handle SDL events
        while (SDL_PollEvent(&event)) {
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
                case SDL_KEYDOWN:
                        switch (event.key.keysym.sym) {
                            case SDLK_LEFT:
                                frame_time = av_frame_get_best_effort_timestamp(pFrame) - frame_increment;
                                printf("%ld\n", frame_time);

                                if (frame_time < 0) {
                                    frame_time = 0;
                                }
                                
                                av_seek_frame(pFormatCtx, videoStream, frame_time, AVSEEK_FLAG_BACKWARD);
                                avcodec_flush_buffers(pCodecCtx);
                                break;
                            case SDLK_RIGHT:
                                frame_time = av_frame_get_best_effort_timestamp(pFrame) + frame_increment;

                                if (frame_time > pFormatCtx->streams[videoStream]->duration) {
                                    frame_time = pFormatCtx->streams[videoStream]->duration;
                                }

                                av_seek_frame(pFormatCtx, videoStream, frame_time, AVSEEK_FLAG_ANY);
                                avcodec_flush_buffers(pCodecCtx);
                                break;
                            case SDLK_SPACE:
                                is_paused = !is_paused;
                                break;
                            default:
                                break;
                        }
                        break;
                default:
                    break;
            }
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