#ifndef LGX2USERSPACE_LGXDEVICE_H
#define LGX2USERSPACE_LGXDEVICE_H

#include <functional>
#include <cstdint>
#include <chrono>
#include "lgxinterfaces.h"
#include "CapturePipeline.h"

namespace lgx2 {

    // The standalone viewer: runs a Stream into a CapturePipeline feeding one video and one audio
    // output, driven by the application's main loop calling run().
    class Device {
    public:
        Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger, ErrorSink *errorSink);

        bool isDeviceAvailable(DeviceType deviceType);

        void initialise(DeviceType deviceType, VideoScale videoScale);

        void run();

        void shutdown();

    private:
        Stream *_stream;
        VideoOutput *_videoOutput;
        AudioOutput *_audioOutput;
        Logger *_logger;
        ErrorSink *_errorSink;

        CapturePipeline _pipeline;

        std::function<void(uint8_t *, uint32_t, bool)> _onFrameData;

        uint64_t _statsFrames{0};
        std::chrono::steady_clock::time_point _statsTimestamp{};

        void reportStats(std::chrono::steady_clock::time_point now);
    };

}

#endif
