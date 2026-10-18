#ifndef LGX2USERSPACE_LGXDEVICE_H
#define LGX2USERSPACE_LGXDEVICE_H

#include <functional>
#include <cstdint>
#include <string>
#include <chrono>
#include <vector>
#include "AudioGate.h"
#include "BufferReorderer.h"
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

        // Callback receives each USB chunk as (data, length, gapped). gapped=true means
        // bytes were lost before this chunk (dropped queue slot, failed or short transfer),
        // so the parser must re-sync before trusting it again.
        virtual void queueFrameRead(std::function<void(uint8_t *frameData, uint32_t length, bool gapped)> *onData) = 0;

        virtual void update() = 0;

        virtual void shutdownStream() = 0;

        // Diagnostics (cumulative since stream start). Default 0 for streams that do not track them.
        virtual uint64_t droppedChunks() const { return 0; }

        virtual uint64_t transferErrors() const { return 0; }

        // Whether the HDMI input currently has a usable signal (cable, TMDS lock, timing).
        // Streams that cannot tell report true, so nothing is muted.
        virtual bool signalPresent() const { return true; }
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

        // The input has no signal: stop showing the last (or a garbage) frame until a new frame
        // arrives. Outputs that cannot blank may ignore it.
        virtual void showNoSignal() {}

    private:
    };

    class AudioOutput {
    public:
        virtual void initialiseAudio() = 0;

        // Audio packets are length-declared on the wire, so byteLength varies per packet.
        virtual void audioFrameAvailable(const uint8_t *audio, uint32_t byteLength) = 0;

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

        utils::BufferReorderer _bufferReorderer;
        utils::FrameBuilder _frameBuilder;
        uint32_t _reportedOrderChanges{0};

        utils::AudioGate _audioGate;
        bool _audioGateReportedOpen{false};
        std::vector<uint8_t> _audioScratch;
        bool _videoBlanked{false};

        void updateSignalState();

        std::function<void(uint8_t *, uint32_t, bool)> _onFrameData;

        uint64_t _videoFrameCount{0};
        std::chrono::steady_clock::time_point _fpsTimestamp{};

        void onFrameData(uint8_t *data, uint32_t length, bool gapped);

        void produceVideoData(uint32_t frameSize, const uint8_t *data);

        void produceAudioData(const uint8_t *data, uint32_t byteLength);
    };

}

#endif
