#include <iostream>
#include <SDL.h>
#include <getopt.h>
#include "UsbStream.h"
#include "SdlFrameOutput.h"

#include "version.h"
#include "V4LFrameOutput.h"
#include "PulseAudioOutput.h"
#include "NOOPLogger.h"
#include "ChronoLogger.h"
#include "OptionParser.h"

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
        std::cout << "Defaulting to SDL2 video output" << std::endl;
        videoOutput = &sdlOutput;
    }

    if (audioOutput == nullptr) {
        std::cout << "Defaulting to SDL2 audio output" << std::endl;
        audioOutput = &sdlOutput;
    }

    if (logger == nullptr) {
        std::cout << "Defaulting to NOOP Logger" << std::endl;
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

    logger->summarise();

    return 0;
}
