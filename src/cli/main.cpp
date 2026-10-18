
#include <iostream>
#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <liblgx.h>
#include "OptionParser.h"
#include "../version.h"

#ifdef __linux__
#include <vector>
#include <v4l/V4LCaptureSource.h>
#ifdef LGX2_VIEWER_PIPEWIRE
#include <pw/PipeWireAudioSource.h>
#endif
#endif

bool do_exit = false;

#ifdef __linux__
// Show what lgx2d is capturing: video from its V4L2 loopback device, audio from its PipeWire
// source. lgx2d owns the LGX2, so nothing here touches USB.
static int viewDaemonOutput(app::OptionParser &options) {
    std::string path = options.inputDevice().empty() ? v4l::V4LCaptureSource::find() : options.inputDevice();
    if (path.empty()) {
        fprintf(stderr, "No video device named LGX2 is offering video - is lgx2d running?\n"
                        "(Use -i to name the device, or -D to capture from the LGX2 directly.)\n");
        return 1;
    }

    lgx2::VideoOutput *videoOutput{options.videoOutput()};
    lgx2::AudioOutput *audioOutput{options.audioOutput()};
    if (videoOutput == nullptr) videoOutput = new sdl::SdlVideoOutput{};
    if (audioOutput == nullptr) audioOutput = new sdl::SdlAudioOutput{};

    try {
        v4l::V4LCaptureSource source{path};
        printf("Viewing lgx2d video from %s\n", path.c_str());
        videoOutput->initialiseVideo(options.scale());
        audioOutput->initialiseAudio();

#ifdef LGX2_VIEWER_PIPEWIRE
        // Called on PipeWire's thread; the audio outputs accept data from any thread.
        pipewire::PipeWireAudioSource audioSource{[audioOutput](const uint8_t *pcm, uint32_t length) {
            audioOutput->audioFrameAvailable(pcm, length);
        }};
        audioSource.start();
        printf("Listening to lgx2d audio (PipeWire source \"LGX2 Audio\")\n");
#else
        printf("Built without PipeWire: no audio\n");
#endif

        signal(SIGTERM, [](int) { do_exit = true; });
        std::vector<uint8_t> frame(v4l::V4LCaptureSource::FRAME_BYTES);
        SDL_Event event;
        while (!do_exit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) do_exit = true;
            }
            // Waiting briefly for a frame also paces the loop.
            if (source.readFrame(frame.data(), 5)) {
                videoOutput->videoFrameAvailable(reinterpret_cast<uint32_t *>(frame.data()));
            }
            videoOutput->display();
            audioOutput->render();
        }

#ifdef LGX2_VIEWER_PIPEWIRE
        audioSource.stop();
#endif
    } catch (const std::exception &e) {
        fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }

    videoOutput->shutdownVideo();
    audioOutput->shutdownAudio();
    return 0;
}
#endif

int main(int argc, char **argv) {
    std::cout << "lgx2userspace-sdl " << APP_VERSION << " ("<< GIT_BRANCH << "-" << GIT_REV << " - " << GIT_TAG << ")" << std::endl;

    app::OptionParser optionParser{};

    if (!optionParser.process(argc, argv)) {
        return 0;
    }

#ifdef __linux__
    if (!optionParser.direct()) {
        return viewDaemonOutput(optionParser);
    }
#endif

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
    lgx2::Device device{stream, videoOutput, audioOutput, logger, errorSink};

    lgx2::DeviceType targetDevice = optionParser.deviceType();

    device.initialise(targetDevice, optionParser.scale());

    signal(SIGTERM, [](int) {
        do_exit = true;
    });

    SDL_Event event;

    try {
        while (!do_exit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    do_exit = true;
                }
            }
            device.run();
        }
    } catch (const std::exception &e) {
        fprintf(stderr, "Fatal: %s\n", e.what());
    }

    device.shutdown();

    logger->summarise();

    return 0;
}
