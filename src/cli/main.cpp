
#include <iostream>
#include <csignal>
#include <liblgx.h>
#include "OptionParser.h"
#include "../version.h"

bool do_exit = false;

int main(int argc, char **argv) {
    std::cout << "lgx2userspace-sdl " << APP_VERSION << " ("<< GIT_BRANCH << "-" << GIT_REV << " - " << GIT_TAG << ")" << std::endl;

    app::OptionParser optionParser{};

    if (!optionParser.process(argc, argv)) {
        return 0;
    }

    lgx2::Logger *logger{optionParser.logger()};
    lgx2::VideoOutput *videoOutput{optionParser.videoOutput()};
    lgx2::AudioOutput *audioOutput{optionParser.audioOutput()};
    lgx2::Stream *stream{optionParser.stream()};

    if (stream == nullptr) {
        stream = new libusb::UsbStream{};
    }

    if (videoOutput == nullptr) {
        videoOutput = new sdl::SdlVideoOutput{};
    }

    if (audioOutput == nullptr) {
        audioOutput = new sdl::SdlAudioOutput{};
    }

    if (logger == nullptr) {
        logger = new NOOPLogger{};
    }

#ifdef __MINGW32__
    lgx2::ErrorSink *errorSink = new error::WindowsErrorSink();
#else
    lgx2::ErrorSink *errorSink = new error::SimpleErrorSink();
#endif
    lgx2::Device device{stream, videoOutput, audioOutput, logger, errorSink};

    lgx2::DeviceType targetDevice = optionParser.deviceType();

    device.initialise(targetDevice, optionParser.scale());

    signal(SIGTERM, [](int) {
        do_exit = true;
    });

    SDL_Event event;

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
