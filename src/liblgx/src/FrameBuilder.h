#ifndef LGX2USERSPACE_FRAMEBUILDER_H
#define LGX2USERSPACE_FRAMEBUILDER_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

namespace utils {
    // Byte-level state-machine parser for the USB stream, mirroring the marker
    // detection in avmu3_x64.sys (ParseAVerData):
    //  - raw byte scan for `00 FF FF T` (T = C0/C1/58), no word-alignment assumption
    //  - straddle reassembly of markers split across chunk boundaries
    //  - checksum-validated SAV/EAV headers with b0/c0 pairing (desync counter)
    //  - length-declared audio packets (big-endian uint32 payload length),
    //    tracked across chunk boundaries — the AA5555AA end marker is never scanned
    //  - frame completion at an EAV boundary once enough video bytes accumulated
    class FrameBuilder {
    public:
        using VideoFrameCallback = std::function<void(const uint8_t *, uint32_t)>;
        using AudioFrameCallback = std::function<void(const uint8_t *, uint32_t)>;

        static constexpr uint32_t VIDEO_FRAME_BYTES = 1920 * 1080 * 2; // YUY2 1080p
        // Each SAV->EAV pair carries one complete frame. Only emit spans within this many bytes
        // of exactly one frame (rejects startup partials and desynced oversized spans).
        static constexpr uint32_t FRAME_SIZE_TOLERANCE = 0x4000;
        static constexpr uint32_t AUDIO_MAX_PACKET_BYTES = 256 * 1024;

        FrameBuilder();
        ~FrameBuilder();

        // Feed one USB chunk. gapped=true means bytes were lost before this chunk
        // (dropped queue slot, failed or short transfer): parser state is reset and
        // a clean SAV->EAV pair (valid checksums, c0 == b0) is required again before
        // frames are emitted.
        void feed(uint8_t *chunk, uint32_t length, bool gapped);

        void setVideoFrameCallback(VideoFrameCallback callback);
        void setAudioFrameCallback(AudioFrameCallback callback);

        uint32_t desyncCount() const;
        uint64_t videoFramesEmitted() const;
        uint64_t audioFramesEmitted() const;

    private:
        enum class State { Idle, VideoData, AudioData };

        static bool savHeaderValid(const uint8_t *b);
        static bool eavHeaderValid(const uint8_t *c);

        void resetForGap();
        void flushVideo(uint32_t start, uint32_t end);
        void consumeAudio(const uint8_t *buf, uint32_t n, uint32_t &pos);
        void checkFrameCompletion();
        void ensureWorkCapacity(uint32_t required);

        // Attempt to fully process a marker at buf[pos] (type byte at pos+3).
        // Returns the scan position to resume from, or -1 when the header runs past
        // the end of the buffer (the caller carries the marker + partial header over).
        int32_t processMarker(const uint8_t *buf, uint32_t n, uint32_t pos);

        State _state{State::Idle};
        bool _armed{false}; // frames only emitted after a clean SAV->EAV pair (post-gap / startup)

        // Two video buffers round-robined on emission: the callback hands out a pointer
        // into the buffer just emitted, and later sub-chunks (which belong to the next
        // frame) must not clobber it before the consumer has copied it. Consumers are
        // synchronous (they copy the frame inside the callback).
        static constexpr uint32_t VIDEO_BUFFER_COUNT = 2;
        static constexpr uint32_t VIDEO_BUFFER_CAPACITY = VIDEO_FRAME_BYTES + 0x80000;
        uint8_t *_videoBuffers[VIDEO_BUFFER_COUNT];
        uint32_t _videoBufferIndex{0};
        uint32_t _videoOffset{0};
        uint32_t _videoPendingStart{0}; // start of unflushed video data in the current work buffer

        std::vector<uint8_t> _audioFrame;
        uint32_t _audioRemaining{0}; // bytes of the current audio payload not yet consumed
        uint32_t _audioOffset{0};    // bytes accumulated in _audioFrame

        uint8_t _savB0{0}; // first byte of last valid SAV header (paired against EAV c0)

        static constexpr uint32_t MAX_CARRY = 16; // marker (4) + longest header (5), plus margin
        uint8_t _carry[MAX_CARRY];
        uint32_t _carryLen{0};

        uint8_t *_work;
        uint32_t _workCapacity{0};

        uint32_t _desyncCount{0};
        uint64_t _videoFramesEmitted{0};
        uint64_t _audioFramesEmitted{0};

        VideoFrameCallback _videoCallback;
        AudioFrameCallback _audioCallback;
    };
}


#endif //LGX2USERSPACE_FRAMEBUILDER_H
