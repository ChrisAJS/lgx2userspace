#ifndef LGX2USERSPACE_OPTIONPARSER_H
#define LGX2USERSPACE_OPTIONPARSER_H

#include <liblgx.h>

namespace app {
    class OptionParser {
    public:
        bool process(int argc, char **argv);

        lgx2::VideoOutput *videoOutput();
        lgx2::AudioOutput *audioOutput();
        lgx2::Logger *logger();
        lgx2::DeviceType deviceType();
        lgx2::Stream *stream();

        lgx2::VideoScale scale();

        // Capture from the LGX2 directly (-D, or implied by -d/-f/-x) rather than viewing lgx2d's
        // output. Always true where lgx2d is not available (non-Linux).
        bool direct();
        // lgx2d's V4L2 device to view (-i); empty to find it by name.
        const std::string &inputDevice();

    private:
        lgx2::VideoOutput *_videoOutput;
        lgx2::AudioOutput *_audioOutput;
        lgx2::Logger *_logger;
        lgx2::DeviceType _deviceType{lgx2::DeviceType::LGX2};
        lgx2::Stream *_stream{nullptr};
        lgx2::VideoScale _scale{lgx2::VideoScale::Full};
#ifdef __linux__
        bool _direct{false};
#else
        bool _direct{true};
#endif
        std::string _inputDevice;
    };
}


#endif //LGX2USERSPACE_OPTIONPARSER_H
