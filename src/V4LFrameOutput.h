#ifndef LGX2USERSPACE_V4LFRAMEOUTPUT_H
#define LGX2USERSPACE_V4LFRAMEOUTPUT_H

#include <string>
#include "lgxdevice.h"

namespace v4l {
    class V4LFrameOutput : public lgx2::FrameOutput {
    public:
        V4LFrameOutput(const std::string &deviceName);

        void videoFrameAvailable(uint32_t *image) override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;

    private:
        int _v4l2fd;
        uint8_t *_frameBuffer;
    };
}


#endif //LGX2USERSPACE_V4LFRAMEOUTPUT_H
