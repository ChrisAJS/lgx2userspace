#ifndef LGX2USERSPACE_OPTIONPARSER_H
#define LGX2USERSPACE_OPTIONPARSER_H

#include "../liblgx/lgxdevice.h"
#include "usb/UsbStream.h"

namespace app {
    class OptionParser {
    public:
        bool process(int argc, char **argv);

        lgx2::VideoOutput *videoOutput();
        lgx2::AudioOutput *audioOutput();
        lgx2::Logger *logger();
        lgx2::DeviceType deviceType();

    private:
        lgx2::VideoOutput *_videoOutput;
        lgx2::AudioOutput *_audioOutput;
        lgx2::Logger *_logger;
        lgx2::DeviceType _deviceType{lgx2::DeviceType::LGX2};
    };
}


#endif //LGX2USERSPACE_OPTIONPARSER_H
