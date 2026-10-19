#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <getopt.h>
#include <string>

#include <memory>
#include <v4l/V4LFrameOutput.h>
#ifdef LGX2D_PIPEWIRE
#include <pw/PipeWireAudioOutput.h>
#endif
#include "CaptureDaemon.h"
#include "../version.h"

namespace {
    std::atomic<bool> stopRequested{false};

    void usage(const char *program) {
        printf("%s usage:\n"
               "\t-d V4L2LoopbackDevice\tV4L2 loopback device to output video to (e.g. /dev/video10), required\n"
               "\t-x\tCapture from an LGX (GC550) instead of an LGX2 (GC551); needs GC550 support compiled in\n"
               "\t-p\tDisable the HDMI input status poll\n"
               "\t-A\tDisable audio output (PipeWire source)\n"
               "\t-h\tPrint this usage message\n", program);
    }
}

int main(int argc, char **argv) {
    printf("lgx2d %s (%s-%s - %s)\n", APP_VERSION, GIT_BRANCH, GIT_REV, GIT_TAG);

    std::string videoDevice;
    lgx2d::CaptureDaemon::Options options;
    bool audioEnabled = true;
    for (int opt; (opt = getopt(argc, argv, "d:xpAh")) != -1;) {
        switch (opt) {
            case 'd':
                videoDevice = optarg;
                break;
            case 'x':
#ifdef GC550_SUPPORT
                options.deviceType = lgx2::DeviceType::LGX;
                break;
#else
                fprintf(stderr, "LGX (GC550) support has not been compiled in. Check the README.md\n");
                return EXIT_FAILURE;
#endif
            case 'p':
                options.statusPoll = false;
                break;
            case 'A':
                audioEnabled = false;
                break;
            default:
                usage(argv[0]);
                return opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
    if (videoDevice.empty()) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, [](int) { stopRequested = true; });
    std::signal(SIGTERM, [](int) { stopRequested = true; });

    try {
        v4l::V4LFrameOutput video{videoDevice};
        video.initialiseVideo(lgx2::VideoScale::Full);
        printf("Video output: %s\n", videoDevice.c_str());

        std::unique_ptr<lgx2::AudioOutput> audio;
#ifdef LGX2D_PIPEWIRE
        if (audioEnabled) {
            audio = std::make_unique<pipewire::PipeWireAudioOutput>();
            audio->initialiseAudio();
        }
#else
        if (audioEnabled) printf("Built without PipeWire: no audio output\n");
#endif

        {
            lgx2d::CaptureDaemon daemon{&video, audio.get(), options};
            daemon.run(stopRequested);
        }

        if (audio) audio->shutdownAudio();
        video.shutdownVideo();
    } catch (const std::exception &e) {
        fprintf(stderr, "Fatal: %s\n", e.what());
        return EXIT_FAILURE;
    }

    printf("Stopped\n");
    return EXIT_SUCCESS;
}
