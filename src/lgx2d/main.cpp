#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <getopt.h>
#include <string>

#include <v4l/V4LFrameOutput.h>
#include "CaptureDaemon.h"
#include "../version.h"

namespace {
    std::atomic<bool> stopRequested{false};

    void usage(const char *program) {
        printf("%s usage:\n"
               "\t-d V4L2LoopbackDevice\tV4L2 loopback device to output video to (e.g. /dev/video10), required\n"
               "\t-p\tDisable the HDMI input status poll\n"
               "\t-h\tPrint this usage message\n", program);
    }
}

int main(int argc, char **argv) {
    printf("lgx2d %s (%s-%s - %s)\n", APP_VERSION, GIT_BRANCH, GIT_REV, GIT_TAG);

    std::string videoDevice;
    lgx2d::CaptureDaemon::Options options;
    for (int opt; (opt = getopt(argc, argv, "d:ph")) != -1;) {
        switch (opt) {
            case 'd':
                videoDevice = optarg;
                break;
            case 'p':
                options.statusPoll = false;
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

        {
            lgx2d::CaptureDaemon daemon{&video, options};
            daemon.run(stopRequested);
        }

        video.shutdownVideo();
    } catch (const std::exception &e) {
        fprintf(stderr, "Fatal: %s\n", e.what());
        return EXIT_FAILURE;
    }

    printf("Stopped\n");
    return EXIT_SUCCESS;
}
