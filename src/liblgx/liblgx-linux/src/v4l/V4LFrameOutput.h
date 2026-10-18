#ifndef LGX2USERSPACE_V4LFRAMEOUTPUT_H
#define LGX2USERSPACE_V4LFRAMEOUTPUT_H

#include <string>
#include <lgxinterfaces.h>

namespace v4l {
    // Writes 1080p YUYV frames to a V4L2 output device (e.g. a v4l2loopback device), one write
    // per frame as it arrives.
    class V4LFrameOutput : public lgx2::VideoOutput {
    public:
        explicit V4LFrameOutput(const std::string &deviceName);

        void initialiseVideo(lgx2::VideoScale scale) override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

    private:
        static constexpr int WIDTH = 1920;
        static constexpr int HEIGHT = 1080;
        static constexpr int FRAME_BYTES = WIDTH * HEIGHT * 2;

        std::string _deviceName;
        int _v4l2fd{-1};
        bool _reportedShortWrite{false};
    };
}


#endif //LGX2USERSPACE_V4LFRAMEOUTPUT_H
