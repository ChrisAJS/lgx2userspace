#ifndef LGX2USERSPACE_CAPTUREPIPELINE_H
#define LGX2USERSPACE_CAPTUREPIPELINE_H

#include <chrono>
#include <cstdint>
#include <vector>
#include "lgxinterfaces.h"
#include "AudioGate.h"
#include "BufferReorderer.h"
#include "FrameBuilder.h"
#include "NoSignalCard.h"

namespace lgx2 {
    // Turns raw USB chunks into video frames and audio packets for one VideoOutput and one
    // AudioOutput (either may be null):
    //
    //   chunks -> BufferReorderer -> FrameBuilder -> video frames  -> VideoOutput
    //                                            \-> audio packets -> AudioGate -> AudioOutput
    //
    // The caller reports the HDMI input state regularly. Without a signal, parsed frames are
    // withheld, showNoSignal() is called on the video output, and audio is gated to silence.
    // With keepAliveFrames set, a no-signal card is also pushed as YUY2 frames at a steady rate,
    // for outputs such as a V4L2 device whose consumers expect frames to keep coming.
    //
    // Time is passed in rather than read inside, so the pipeline can be tested without waiting.
    class CapturePipeline {
    public:
        using Clock = std::chrono::steady_clock;

        struct Options {
            bool keepAliveFrames{false};
            std::chrono::milliseconds keepAliveInterval{33}; // ~30 fps
        };

        CapturePipeline(VideoOutput *video, AudioOutput *audio, Options options);

        // Feed one raw USB chunk; gapped=true means bytes were lost before it.
        void feed(uint8_t *chunk, uint32_t length, bool gapped);

        // Report the HDMI input state; call regularly, not only when data arrives (a disconnected
        // input may stop delivering data altogether).
        void setInputState(InputState state, Clock::time_point now);

        // Emits keep-alive frames when due; call regularly.
        void tick(Clock::time_point now);

        uint64_t videoFramesEmitted() const;   // captured frames passed to the video output
        uint64_t keepAliveFramesEmitted() const;
        uint32_t desyncCount() const;

    private:
        VideoOutput *_video;
        AudioOutput *_audio;
        Options _options;

        utils::BufferReorderer _bufferReorderer;
        utils::FrameBuilder _frameBuilder;
        uint32_t _reportedOrderChanges{0};

        InputState _inputState{InputState::Signal};
        utils::AudioGate _audioGate;
        bool _audioGateReportedOpen{false};
        std::vector<uint8_t> _audioScratch;

        bool _videoBlanked{false};
        InputState _blankedFor{InputState::Signal};

        NoSignalCard _card;
        Clock::time_point _lastKeepAlive{};

        uint64_t _videoFramesEmitted{0};
        uint64_t _keepAliveFramesEmitted{0};

        void onVideoFrame(const uint8_t *data);
        void onAudioPacket(const uint8_t *data, uint32_t byteLength);
    };
}

#endif //LGX2USERSPACE_CAPTUREPIPELINE_H
