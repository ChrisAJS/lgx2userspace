#ifndef LGX2USERSPACE_V4LCAPTURESOURCE_H
#define LGX2USERSPACE_V4LCAPTURESOURCE_H

#include <cstdint>
#include <string>

namespace v4l {
    // Reads 1080p YUYV frames from a V4L2 capture device, such as the loopback device lgx2d writes
    // to.
    class V4LCaptureSource {
    public:
        static constexpr int WIDTH = 1920;
        static constexpr int HEIGHT = 1080;
        static constexpr int FRAME_BYTES = WIDTH * HEIGHT * 2;

        // Path of the V4L2 capture device named `cardName`, or "" if there is none. A loopback
        // device created with exclusive_caps=1 only offers capture while something is writing to
        // it, so this also tells whether lgx2d is running.
        static std::string find(const std::string &cardName = "LGX2");

        // Opens the device and checks it delivers 1080p YUYV; throws otherwise.
        explicit V4LCaptureSource(const std::string &path);
        ~V4LCaptureSource();
        V4LCaptureSource(const V4LCaptureSource &) = delete;
        V4LCaptureSource &operator=(const V4LCaptureSource &) = delete;

        // Waits up to timeoutMs for a frame; true when `frame` (FRAME_BYTES long) was filled.
        bool readFrame(uint8_t *frame, int timeoutMs);

        const std::string &path() const { return _path; }

    private:
        // Streaming I/O with memory-mapped buffers, as OBS and ffplay use (v4l2loopback never
        // reports a frame ready to poll() before the first read(), and both are equally exclusive).
        static constexpr unsigned BUFFER_COUNT = 4;

        struct MappedBuffer {
            void *start{nullptr};
            size_t length{0};
        };

        std::string _path;
        int _fd{-1};
        MappedBuffer _buffers[BUFFER_COUNT];
        unsigned _bufferCount{0};
        bool _streaming{false};

        void startStreaming();
        void release();
    };
}

#endif //LGX2USERSPACE_V4LCAPTURESOURCE_H
