# h264Player
A project for h.264 muxer &amp; player, using FFmepg library.

*All usage is for training myself.*

You need to install [FFmepg](https://github.com/FFmpeg/FFmpeg) & [SDL2](https://github.com/libsdl-org/SDL) lib first.
Or by apt-get install :
```bash
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev
```
```bash
sudo apt-get install libsdl2-dev
```

Then compile by following command to link your cpp program and libs.

muxing.cpp
```bash
g++ -o muxing muxing.cpp -lavutil -lavformat -lavcodec -lswscale -lswresample
```

h264Streamer.cpp
```bash
g++ -o h264Streamer h264Streamer.cpp -lavformat -lavcodec -lavutil -lswscale -lswresample -lSDL2 -lm -lpthread
./h264Streamer <file>
```
