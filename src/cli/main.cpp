
#include <iostream>
#include <liblgx.h>
#include "OptionParser.h"
#include "../version.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>


SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
    std::cout << "lgx2userspace-sdl " << APP_VERSION << " ("<< GIT_BRANCH << "-" << GIT_REV << " - " << GIT_TAG << ")" << std::endl;

    app::OptionParser optionParser{};

    if (!optionParser.process(argc, argv)) {
        return SDL_APP_FAILURE;
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
#elifdef __APPLE__
    lgx2::ErrorSink *errorSink = new error::MacOsErrorSink();
#else
    lgx2::ErrorSink *errorSink = new error::SimpleErrorSink();
#endif
    lgx2::Device *device = new lgx2::Device{stream, videoOutput, audioOutput, logger, errorSink};

    const lgx2::DeviceType targetDevice = optionParser.deviceType();

    device->initialise(targetDevice, optionParser.scale());

    *appstate = device;

    logger->summarise();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    lgx2::Device *device = static_cast<lgx2::Device *>(appstate);
    device->run();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult
    result) {
    lgx2::Device *device = static_cast<lgx2::Device *>(appstate);

    device->shutdown();
    delete device;
}