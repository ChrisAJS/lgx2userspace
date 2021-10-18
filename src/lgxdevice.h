#ifndef LGX2USERSPACE_LGXDEVICE_H
#define LGX2USERSPACE_LGXDEVICE_H

#include <functional>
#include <cstdint>
#include <string>
#include "FrameBuilder.h"

namespace lgx2 {

    class Logger {
    public:
        virtual void logTimeStart(const std::string &name) = 0;

        virtual void logTimeEnd(const std::string &name, const std::string &message) = 0;

        virtual void summarise() = 0;
    };

    class Stream {
    public:
        virtual void streamSetupCommands() = 0;

        virtual void queueFrameRead(std::function<void(uint8_t *frameData)> *onData) = 0;

        virtual void update() = 0;

        virtual void shutdownStream() = 0;
    };

    class VideoOutput {
    public:
        virtual void initialiseVideo() = 0;

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
        Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger);

        void initialise();

        void run();

        void shutdown();

    private:
        Stream *_stream;
        VideoOutput *_videoOutput;
        AudioOutput *_audioOutput;
        Logger *_logger;

        utils::FrameBuilder _frameBuilder;

        std::function<void(uint8_t *)> _onFrameData;

        void onFrameData(uint8_t *data);

        void produceVideoData(uint8_t *data);

        void produceAudioData(uint8_t *data);
    };

}

#endif
