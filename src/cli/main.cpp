
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

    lgx2::Stream *stream{optionParser.stream()};
    sdl::SdlFrameOutput sdlOutput{};
    NOOPLogger noopLogger{};

    if (stream == nullptr) {
        stream = new libusb::UsbStream();
    }

    if (videoOutput == nullptr) {
        videoOutput = &sdlOutput;
    }

    if (audioOutput == nullptr) {
        audioOutput = &sdlOutput;
    }

    if (logger == nullptr) {
        logger = &noopLogger;
    }

    lgx2::Device device{stream, videoOutput, audioOutput, logger};

    lgx2::DeviceType targetDevice = optionParser.deviceType();

    if (!device.isDeviceAvailable(targetDevice)) {
        throw std::runtime_error("Target device is not available to use - is it plugged in?");
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
