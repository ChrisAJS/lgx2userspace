#ifndef LGX2USERSPACE_PIPEWIREAUDIOOUTPUT_H
#define LGX2USERSPACE_PIPEWIREAUDIOOUTPUT_H

#include <atomic>
#include <string>
#include <vector>
#include <lgxinterfaces.h>

struct pw_thread_loop;
struct pw_stream;

namespace pipewire {
    // Publishes the captured audio as a PipeWire source node ("LGX2 Audio"), which recording
    // applications such as OBS can capture from like a microphone; any number of them at once.
    //
    // Packets arrive at the capture card's rate and PipeWire pulls at its own graph rate, so a
    // lock-free ring sits between them, kept near TARGET_MS of audio: trimmed back when it grows
    // past MAX_MS (e.g. stale audio queued before a consumer connected), refilled with silence
    // played meanwhile when it runs dry.
    class PipeWireAudioOutput : public lgx2::AudioOutput {
    public:
        explicit PipeWireAudioOutput(std::string nodeName = "lgx2_audio", std::string description = "LGX2 Audio");
        ~PipeWireAudioOutput();

        void initialiseAudio() override;

        void audioFrameAvailable(const uint8_t *audio, uint32_t byteLength) override;

        void render() override;

        void shutdownAudio() override;

    private:
        // The card's audio format: 48 kHz, 16-bit little-endian stereo.
        static constexpr uint32_t RATE = 48000;
        static constexpr uint32_t CHANNELS = 2;
        static constexpr uint32_t STRIDE = CHANNELS * 2;

        static constexpr uint32_t RING_BYTES = 1u << 16; // ~340 ms; must be a power of two
        static constexpr uint32_t TARGET_MS = 40;
        static constexpr uint32_t MAX_MS = 120;
        static constexpr uint32_t TARGET_BYTES = RATE * STRIDE * TARGET_MS / 1000;
        static constexpr uint32_t MAX_BYTES = RATE * STRIDE * MAX_MS / 1000;

        std::string _nodeName;
        std::string _description;

        pw_thread_loop *_loop{nullptr};
        pw_stream *_stream{nullptr};

        // spa_ringbuffer state (read/write indices) plus its storage. Written by the capture
        // thread, read by PipeWire's real-time thread.
        struct Ring;
        Ring *_ring{nullptr};
        std::vector<uint8_t> _ringData;

        bool _prebuffering{true}; // only touched by the real-time thread
        std::atomic<uint64_t> _underruns{0};
        std::atomic<uint64_t> _trims{0};
        std::atomic<uint64_t> _droppedPackets{0};

        static void onProcess(void *userData);
    };
}

#endif //LGX2USERSPACE_PIPEWIREAUDIOOUTPUT_H
