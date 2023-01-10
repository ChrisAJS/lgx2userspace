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

    private:
        lgx2::VideoOutput *_videoOutput;
        lgx2::AudioOutput *_audioOutput;
        lgx2::Logger *_logger;
        lgx2::DeviceType _deviceType{lgx2::DeviceType::LGX2};
        lgx2::Stream *_stream{nullptr};
        lgx2::VideoScale _scale{lgx2::VideoScale::Full};
    };
}


#endif //LGX2USERSPACE_OPTIONPARSER_H
