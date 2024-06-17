# h264Player
A project for h.264 muxer &amp; player, using FFmepg library.
*All usage is for training myself.*

You need to install [FFmepg](https://github.com/FFmpeg/FFmpeg) & [SDL2](https://github.com/libsdl-org/SDL) lib first.
Or by apt-get install :
```bash
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev
```

Then compile by following command to link your c program and libs.
```bash
gcc -o h264_player h264_player.c -lavformat -lavcodec -lavutil -lswscale -lswresample -lSDL2 -lm -lpthread
```
