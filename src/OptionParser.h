#ifndef LGX2USERSPACE_OPTIONPARSER_H
#define LGX2USERSPACE_OPTIONPARSER_H

#include "lgxdevice.h"

namespace app {
    class OptionParser {
    public:
        bool process(int argc, char **argv);

        lgx2::VideoOutput *videoOutput();
        lgx2::AudioOutput *audioOutput();
        lgx2::Logger *logger();

    private:
        lgx2::VideoOutput *_videoOutput;
        lgx2::AudioOutput *_audioOutput;
        lgx2::Logger *_logger;
    };
}


#endif //LGX2USERSPACE_OPTIONPARSER_H
