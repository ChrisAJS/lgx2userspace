#ifndef LGX2USERSPACE_LGXDEVICE_H
#define LGX2USERSPACE_LGXDEVICE_H

#include <functional>
#include <cstdint>
#include <string>
#include <chrono>
#include "FrameBuilder.h"

namespace lgx2 {

    enum class DeviceType {

#ifdef GC550_SUPPORT
        LGX,
#endif
        LGX2
    };

    class ErrorSink {
    public:
        virtual void catchErrors(const std::function<void()> &run) = 0;
    };

    class Logger {
    public:
        virtual void logTimeStart(const std::string &name) = 0;

        virtual void logTimeEnd(const std::string &name, const std::string &message) = 0;

        virtual void summarise() = 0;
    };

    class Stream {
    public:
        virtual ~Stream() = default;
        virtual bool deviceAvailable(DeviceType deviceType) = 0;

        virtual void streamSetupCommands(DeviceType deviceType) = 0;

        virtual void queueFrameRead(std::function<void(uint8_t *frameData)> *onData) = 0;

        virtual void update() = 0;

        virtual void shutdownStream() = 0;
    };

    enum class VideoScale {
        Full,
        Half,
        Quarter
    };

    class VideoOutput {
    public:
        virtual ~VideoOutput() = default;

        virtual void initialiseVideo(VideoScale scale) = 0;

        virtual void videoFrameAvailable(uint32_t *image) = 0;

        virtual void display() = 0;

        virtual void shutdownVideo() = 0;

    private:
    };

    class AudioOutput {
    public:
        virtual void initialiseAudio() = 0;

        virtual void audioFrameAvailable(uint32_t *audio) = 0;

        virtual void render() = 0;

        virtual void shutdownAudio() = 0;

    private:
    };

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

        utils::FrameBuilder _frameBuilder;

        std::function<void(uint8_t *)> _onFrameData;

        // Full 1080p YUY2 frame = 1,036,800 uint32s. C1FFFF00 is a sub-frame chunk
        // delimiter (~16 chunks per frame); only produce once enough has accumulated.
        static constexpr uint32_t MINIMUM_VIDEO_FRAME_SIZE = 1000000;

        int _inAudio{0};

        uint64_t _videoFrameCount{0};
        uint32_t _maxVideoFrameSize{0};
        uint32_t _minVideoFrameSize{UINT32_MAX};
        std::chrono::steady_clock::time_point _fpsTimestamp{};

        void onFrameData(uint8_t *data);

        void produceVideoData(uint32_t frameSize, uint8_t *data);

        void produceAudioData(uint8_t *data);
    };

}

#endif
