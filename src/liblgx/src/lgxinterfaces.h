#ifndef LGX2USERSPACE_LGXINTERFACES_H
#define LGX2USERSPACE_LGXINTERFACES_H

#include <functional>
#include <cstdint>
#include <string>

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

    // State of the HDMI input, as far as the stream can tell.
    enum class InputState {
        Signal,   // usable signal (cable, TMDS lock, timing)
        NoSignal, // cable connected (+5V present) but nothing usable: source off or re-locking
        NoCable,  // no +5V from a source
        NoDevice  // no capture card attached (only reported by callers that outlive the card)
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

        // State of the HDMI input. Streams that cannot tell report Signal, so nothing is muted.
        virtual InputState inputState() const { return InputState::Signal; }

        bool signalPresent() const { return inputState() == InputState::Signal; }
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

        // The input has no signal (reason: anything but Signal): stop showing the last (or a
        // garbage) frame and show that state until a new frame arrives. Called again if the
        // reason changes. Outputs that cannot do this may ignore it.
        virtual void showNoSignal(InputState reason) { (void) reason; }

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

}

#endif //LGX2USERSPACE_LGXINTERFACES_H
