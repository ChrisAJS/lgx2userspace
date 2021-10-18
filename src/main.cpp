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

int main(int argc, char **argv) {
    std::cout << "lgx2userspace v0.0.0 ("<< GIT_BRANCH << "-" << GIT_REV << ")" << std::endl;

    lgx2::Logger *logger{nullptr};
    lgx2::VideoOutput *videoOutput{nullptr};
    lgx2::AudioOutput *audioOutput{nullptr};

    for(;;)
    {
        switch(getopt(argc, argv, "va:d:h"))
        {
            case 'a':
                std::cout << "Attempting to output to Pulseaudio sink: " << optarg << std::endl;
                audioOutput = new pulse::PulseAudioOutput(optarg);
                continue;
            case 'v':
                std::cout <<"Logging diagnostics information" << std::endl;
                logger = new ChronoLogger();
                continue;

            case 'd':
                std::cout << "Attempting to output to V4L2Loopback device: " << optarg << std::endl;
                videoOutput = new v4l::V4LFrameOutput(optarg);
                continue;

            case 'h':
            default :
                std::cout << argv[0] << " usage:\n\t-v\tPrint diagnostics information, useful when submitting bugs\n\t-d V4L2LoopbackDevice\tSpecify the V4L2Loopback device to output video to (e.g. /dev/video99)\n\n";
                return 0;
            case -1:
                break;
        }

        break;
    }

    sdl::SdlFrameOutput sdlOutput = sdl::SdlFrameOutput();

    lgx2::Stream *stream = new libusb::UsbStream();

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
        logger = new NOOPLogger;
    }

    lgx2::Device device{stream, videoOutput, audioOutput, logger};

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
