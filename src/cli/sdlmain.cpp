
#include <iostream>
#include <csignal>
#include <liblgx.h>
#include "OptionParser.h"
#include "../version.h"

bool do_exit = false;

int main(int argc, char **argv) {
    std::cout << "lgx2userspace v0.2.0 ("<< GIT_BRANCH << "-" << GIT_REV << ")" << std::endl;

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

    bool lgxAvailable = device.isDeviceAvailable(lgx2::DeviceType::LGX);
    bool lgx2Available = device.isDeviceAvailable(lgx2::DeviceType::LGX2);
    lgx2::DeviceType targetDevice = optionParser.deviceType();

    if (lgxAvailable && lgx2Available) {
        printf("Unable to auto detect which device to use as both LGX and LGX2 available - will use as indicated by the -x argument.\n");
    } else {
        if (lgxAvailable) {
            targetDevice = lgx2::DeviceType::LGX;
        } else if (lgx2Available) {
            targetDevice = lgx2::DeviceType::LGX2;
        }
    }

    device.initialise(targetDevice);

    SDL_Event event;

    signal(SIGTERM, [](int) {
        do_exit = true;
    });

    while (!do_exit) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                do_exit = true;
            }
        }

        device.run();
    }

    device.shutdown();

    logger->summarise();

    return 0;
}
