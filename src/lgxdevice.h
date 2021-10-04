#ifndef LGX2USERSPACE_LGXDEVICE_H
#define LGX2USERSPACE_LGXDEVICE_H

#include <functional>
#include <cstdint>
#include <thread>

#include "FrameBuilder.h"

namespace lgx2 {

    class Stream {
    public:
        virtual void streamSetupCommands() = 0;

        virtual void queueFrameRead(std::function<void(uint8_t *frameData, uint32_t)> *onData) = 0;

        virtual void update() = 0;
    };

    class FrameOutput {
    public:
        virtual void videoFrameAvailable(uint32_t *image) = 0;

        virtual void audioFrameAvailable(uint32_t *audio) = 0;

        virtual void render() = 0;
    private:
    };

    class Device {
    public:
        Device(Stream *stream, FrameOutput *frameOutput);

        void initialise();

        void run();

    private:
        Stream *_stream;
        FrameOutput *_frameOutput;
        utils::FrameBuilder _frameBuilder;

        std::function<void(uint8_t *, uint32_t)> _onFrameData;

        void onFrameData(uint8_t *data);

        void produceVideoData(uint8_t *data);

        void produceAudioData(uint8_t *data);

        void poll();

        std::thread _pollThread;
    };

}

#endif
