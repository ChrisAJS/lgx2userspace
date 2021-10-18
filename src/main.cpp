#include <iostream>
#include <SDL.h>
#include "UsbStream.h"
#include "SdlFrameOutput.h"
#include "V4LFrameOutput.h"
#include "NOOPLogger.h"
#include "OptionParser.h"
#include "version.h"

int main(int argc, char **argv) {
    std::cout << "lgx2userspace v0.0.0 ("<< GIT_BRANCH << "-" << GIT_REV << ")" << std::endl;

    app::OptionParser optionParser{};

    if (!optionParser.process(argc, argv)) {
        return 0;
    }

    lgx2::Logger *logger{optionParser.logger()};
    lgx2::VideoOutput *videoOutput{optionParser.videoOutput()};
    lgx2::AudioOutput *audioOutput{optionParser.audioOutput()};

    libusb::UsbStream stream{};
    sdl::SdlFrameOutput sdlOutput = sdl::SdlFrameOutput();
    NOOPLogger noopLogger{};

    if (videoOutput == nullptr) {
        videoOutput = &sdlOutput;
    }

    if (audioOutput == nullptr) {
        audioOutput = &sdlOutput;
    }

    if (logger == nullptr) {
        logger = &noopLogger;
    }

    lgx2::Device device{&stream, videoOutput, audioOutput, logger};

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

    device.shutdown();

    logger->summarise();

    return 0;
}
