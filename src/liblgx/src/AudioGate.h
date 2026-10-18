#ifndef LGX2USERSPACE_AUDIOGATE_H
#define LGX2USERSPACE_AUDIOGATE_H

#include <chrono>
#include <cstdint>

namespace utils {
    // Mutes audio while the HDMI input has no usable signal. Without a signal (cable unplugged,
    // source switched off or still re-locking) the stream can keep delivering audio packets full
    // of garbage, which plays as loud crackling.
    //
    // The gate closes as soon as the signal is reported lost and opens only once it has been
    // present continuously for SETTLE_TIME. Gain changes are ramped over RAMP_FRAMES so muting and
    // un-muting do not click. While closed, packets are replaced with silence.
    class AudioGate {
    public:
        using Clock = std::chrono::steady_clock;

        static constexpr uint32_t CHANNELS = 2;                   // 16-bit stereo PCM
        static constexpr uint32_t RAMP_FRAMES = 480;              // 10 ms at 48 kHz
        static constexpr std::chrono::milliseconds SETTLE_TIME{500};

        // Report the current signal state; call before processing each packet.
        void update(bool signalPresent, Clock::time_point now);

        // Apply the gate in place to interleaved little-endian 16-bit samples.
        void process(uint8_t *pcm, uint32_t byteLength);

        bool open() const;

    private:
        bool _open{false};
        bool _present{false};
        Clock::time_point _presentSince{};
        uint32_t _rampPosition{0}; // gain = _rampPosition / RAMP_FRAMES, stepped toward 0 or RAMP_FRAMES
    };
}

#endif //LGX2USERSPACE_AUDIOGATE_H
