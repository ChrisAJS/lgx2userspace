#include <iostream>
#include <SDL.h>
#include "UsbStream.h"
#include "SdlFrameOutput.h"

#include "version.h"
#include "V4LFrameOutput.h"

int main(int argc, char **argv) {
    std::cout << "lgx2userspace v0.0.0 ("<< GIT_BRANCH << "-" << GIT_REV << ")" << std::endl;

    sdl::SdlFrameOutput sdlOutput = sdl::SdlFrameOutput();

    lgx2::VideoOutput *videoOutput;
    lgx2::Stream *stream;

    if (argc > 1) {
        std::cout << "Rendering to V4L2Loopback device " << argv[1] << std::endl;
        videoOutput = new v4l::V4LFrameOutput(argv[1]);
    } else {
        std::cout << "Rendering to SDL screen" << std::endl;
        videoOutput = &sdlOutput;
    }

    stream = new libusb::UsbStream();

    lgx2::Device device{stream, videoOutput, &sdlOutput};

    device.initialise();

    SDL_Event event;
    int do_exit = 0;
    while (!do_exit) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                do_exit = 1;
            }
        }

        device.run();
    }

    return 0;
}
