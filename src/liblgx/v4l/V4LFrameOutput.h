#ifndef LGX2USERSPACE_V4LFRAMEOUTPUT_H
#define LGX2USERSPACE_V4LFRAMEOUTPUT_H

#include <string>
#include "../lgxdevice.h"

namespace v4l {
    class V4LFrameOutput : public lgx2::VideoOutput {
    public:
        V4LFrameOutput(const std::string &deviceName);

        void initialiseVideo() override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

    private:
        int _v4l2fd;
        uint8_t *_frameBuffer;
    };
}


#endif //LGX2USERSPACE_V4LFRAMEOUTPUT_H
