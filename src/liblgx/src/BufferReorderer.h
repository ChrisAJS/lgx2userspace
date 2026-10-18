#ifndef LGX2USERSPACE_BUFFERREORDERER_H
#define LGX2USERSPACE_BUFFERREORDERER_H

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace utils {
    // Restores the order of the FX3's 16 KiB DMA buffers before the stream is parsed.
    //
    // The device delivers the stream in 16 KiB buffers that alternate between two sources.
    // After an unclean stop (e.g. a previous session ended mid-stream) the pairs can arrive
    // swapped, and stay that way until the device is power cycled. Every byte is still there,
    // but each 32 KiB pair is in the wrong order (see PROTOCOL.md, "Swapped DMA buffer order").
    //
    // Detection uses the stream itself. Each SAV starts a DMA buffer, and in swapped order the
    // buffer that should precede it, ending with the previous frame's EAV, is delivered right
    // after it. So a checksum-valid EAV for an earlier frame (c0 != b0) within one buffer pair
    // after a SAV means "swapped, with the pair grid starting at that SAV". No such EAV means
    // normal order. Once established, the order only changes when two consecutive SAVs agree.
    //
    // Assumes a frame period of an even number of DMA buffers (1080p: 254), so every SAV keeps
    // its place in the buffer pair. Other resolutions are untested.
    //
    // Data is held back by about 64 KiB so each SAV's verdict is known before its bytes are passed
    // on. Data before the first decided SAV is dropped, and the first output (and any output
    // after a change of order) is flagged as gapped so the parser re-syncs.
    class BufferReorderer {
    public:
        using Sink = std::function<void(uint8_t *data, uint32_t length, bool gapped)>;

        enum class Order { Unknown, Normal, Swapped };

        static constexpr uint32_t DMA_BUFFER_BYTES = 0x4000;
        static constexpr uint32_t PAIR_BYTES = 2 * DMA_BUFFER_BYTES;
        // How far after a SAV a late EAV (c0 == b0 - 1) may appear for the order to count as swapped.
        static constexpr uint32_t SWAP_WINDOW = PAIR_BYTES;

        void setSink(Sink sink);

        // Feed one USB chunk. gapped=true means bytes were lost before it, so the buffer pair
        // grid is unknown and the order is detected again.
        void feed(const uint8_t *chunk, uint32_t length, bool gapped);

        Order order() const;
        uint32_t orderChanges() const;

    private:
        struct Observation {
            uint64_t savOffset;
            Order order;
        };

        void reset();
        void scan();
        void observe(uint64_t savOffset, Order order);
        void emit(uint64_t limit);
        void applyObservation(const Observation &observation);
        static bool agrees(const Observation &observation, Order order, uint64_t pairOrigin);
        void flushOut();
        const uint8_t *at(uint64_t offset) const;

        Sink _sink;

        // Raw bytes from _pipeStart up to _inOffset (end of input so far). _outOffset is the next
        // byte to emit; bytes before it are erased from the pipe after each feed.
        std::vector<uint8_t> _pipe;
        uint64_t _pipeStart{0};
        uint64_t _outOffset{0};
        uint64_t _inOffset{0};
        uint64_t _scanOffset{0};

        bool _pendingSav{false};
        uint64_t _pendingSavOffset{0};
        uint8_t _pendingSavB0{0};
        std::deque<Observation> _observations;

        Order _order{Order::Unknown};
        uint64_t _pairOrigin{0};
        // A different order seen on consecutive SAVs; it takes effect on the second.
        Order _candidateOrder{Order::Unknown};
        uint64_t _candidateOrigin{0};
        uint32_t _candidateCount{0};
        uint32_t _orderChanges{0};
        bool _gapPending{true};

        std::vector<uint8_t> _out;
    };
}

#endif //LGX2USERSPACE_BUFFERREORDERER_H
