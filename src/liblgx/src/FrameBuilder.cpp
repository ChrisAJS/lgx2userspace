#include <cstring>
#include "FrameBuilder.h"

namespace utils {

    // SAV header (4 bytes after the 00 FF FF C0 marker): valid iff
    // b3 == (b0 + b1 + b2 - 0x40) mod 256. (Verified against a live capture: the checksum
    // holds for every real SAV, but bit 0 of b2 is usually *set*, so it must not be required.)
    bool FrameBuilder::savHeaderValid(const uint8_t *b) {
        return b[3] == static_cast<uint8_t>(b[0] + b[1] + b[2] - 0x40);
    }

    // EAV header (5 bytes after the 00 FF FF C1 marker): valid iff
    // c4 == (c0 + c1 + c2 + c3 - 0x3F) mod 256.
    bool FrameBuilder::eavHeaderValid(const uint8_t *c) {
        return c[4] == static_cast<uint8_t>(c[0] + c[1] + c[2] + c[3] - 0x3F);
    }

    FrameBuilder::FrameBuilder() : _work{nullptr} {
        for (uint32_t i = 0; i < VIDEO_BUFFER_COUNT; i++) {
            _videoBuffers[i] = new uint8_t[VIDEO_BUFFER_CAPACITY];
        }
        ensureWorkCapacity(0x1FC000 + MAX_CARRY);
    }

    FrameBuilder::~FrameBuilder() {
        for (uint32_t i = 0; i < VIDEO_BUFFER_COUNT; i++) {
            delete[] _videoBuffers[i];
        }
        delete[] _work;
    }

    void FrameBuilder::ensureWorkCapacity(uint32_t required) {
        if (required <= _workCapacity) return;
        uint32_t capacity = 0x1FC000 + MAX_CARRY;
        while (capacity < required) capacity *= 2;
        uint8_t *grown = new uint8_t[capacity];
        delete[] _work;
        _work = grown;
        _workCapacity = capacity;
    }

    void FrameBuilder::setVideoFrameCallback(VideoFrameCallback callback) {
        _videoCallback = std::move(callback);
    }

    void FrameBuilder::setAudioFrameCallback(AudioFrameCallback callback) {
        _audioCallback = std::move(callback);
    }

    uint32_t FrameBuilder::desyncCount() const {
        return _desyncCount;
    }

    uint64_t FrameBuilder::videoFramesEmitted() const {
        return _videoFramesEmitted;
    }

    uint64_t FrameBuilder::audioFramesEmitted() const {
        return _audioFramesEmitted;
    }

    void FrameBuilder::resetForGap() {
        _state = State::Idle;
        _armed = false;
        _videoOffset = 0;
        _videoPendingStart = 0;
        _audioRemaining = 0;
        _audioOffset = 0;
        _carryLen = 0;
    }

    void FrameBuilder::flushVideo(uint32_t start, uint32_t end) {
        if (end <= start) return;
        uint8_t *buffer = _videoBuffers[_videoBufferIndex];
        uint32_t bytes = end - start;
        uint32_t room = VIDEO_BUFFER_CAPACITY - _videoOffset;
        if (bytes > room) {
            // Frame buffer overflowed: the stream has desynced, clamp and count it.
            _desyncCount++;
            bytes = room;
        }
        memcpy(buffer + _videoOffset, _work + start, bytes);
        _videoOffset += bytes;
    }

    void FrameBuilder::consumeAudio(const uint8_t *buf, uint32_t n, uint32_t &pos) {
        if (_state != State::AudioData) return;
        // Audio payload is exactly _audioRemaining bytes of opaque data (no marker scan).
        uint32_t available = n - pos;
        uint32_t take = available < _audioRemaining ? available : _audioRemaining;
        if (take > 0) {
            if (_audioOffset + take > _audioFrame.size()) _audioFrame.resize(_audioOffset + take);
            memcpy(_audioFrame.data() + _audioOffset, buf + pos, take);
            _audioOffset += take;
            _audioRemaining -= take;
            pos += take;
        }

        if (_audioRemaining == 0) {
            _state = State::Idle;
            _audioFramesEmitted++;
            if (_audioCallback) _audioCallback(_audioFrame.data(), _audioOffset);
        }
    }

    void FrameBuilder::checkFrameCompletion() {
        // Each valid SAV->EAV pair carries one complete video frame. Emit only when the just-flushed
        // span is (within a quantum of) exactly one 1080p frame: smaller spans are startup partials
        // and larger ones indicate a desync — neither should be shown. _videoOffset holds the span size.
        if (!_armed) return;
        if (_videoOffset < VIDEO_FRAME_BYTES - FRAME_SIZE_TOLERANCE ||
            _videoOffset > VIDEO_FRAME_BYTES + FRAME_SIZE_TOLERANCE) {
            return;
        }
        _videoFramesEmitted++;
        if (_videoCallback) _videoCallback(_videoBuffers[_videoBufferIndex], VIDEO_FRAME_BYTES);
        // Round-robin so the next frame's data does not clobber the buffer just handed to the callback.
        _videoOffset = 0;
        _videoBufferIndex ^= 1;
    }

    int32_t FrameBuilder::processMarker(const uint8_t *buf, uint32_t n, uint32_t pos) {
        const uint8_t type = buf[pos + 3];
        switch (type) {
            case 0xC0: { // SAV: opens a video frame, data starts after marker (4) + header (4)
                if (n - pos < 8) return -1; // header runs past end of buffer
                if (!savHeaderValid(buf + pos + 4)) return pos + 1; // invalid checksum: not a real marker
                _savB0 = buf[pos + 4];
                _state = State::VideoData;
                _videoPendingStart = pos + 8;
                _videoOffset = 0; // each SAV->EAV pair carries one complete frame
                return _videoPendingStart;
            }
            case 0xC1: { // EAV: closes the current video frame
                if (_state != State::VideoData) return pos + 1; // stray EAV outside video data
                if (n - pos < 9) return -1; // marker (4) + header (5) runs past end of buffer
                const uint8_t *c = buf + pos + 4;
                flushVideo(_videoPendingStart, pos);
                if (!eavHeaderValid(c)) {
                    _desyncCount++;
                    _state = State::Idle;
                    return pos + 9;
                }
                _armed = true; // a checksum-valid pair re-arms emission (post-gap / startup)
                if (c[0] != _savB0) _desyncCount++; // soft mismatch: count it, but the data is still a frame
                checkFrameCompletion();
                _state = State::Idle;
                return pos + 9;
            }
            case 0x58: { // audio: big-endian uint32 payload length, then exactly that many bytes
                if (n - pos < 8) return -1; // marker (4) + length header (4) runs past end of buffer
                const uint8_t *l = buf + pos + 4;
                uint32_t payloadLength = ((uint32_t) l[0] << 24) | ((uint32_t) l[1] << 16) |
                                         ((uint32_t) l[2] << 8) | (uint32_t) l[3];
                if (payloadLength > AUDIO_MAX_PACKET_BYTES) return pos + 1; // implausible: not a real marker
                if (_state == State::VideoData) flushVideo(_videoPendingStart, pos); // audio between sub-chunks
                _audioRemaining = payloadLength;
                _audioOffset = 0;
                _state = State::AudioData;
                return pos + 8;
            }
            default:
                return pos + 1; // 00 FF FF <other>: not a marker
        }
    }

    void FrameBuilder::feed(uint8_t *chunk, uint32_t length, bool gapped) {
        if (gapped) resetForGap();
        if (length == 0) return;

        // Assemble the effective buffer: carried tail bytes from the previous chunk, then this one.
        ensureWorkCapacity(_carryLen + length);
        if (_carryLen > 0) memcpy(_work, _carry, _carryLen);
        memcpy(_work + _carryLen, chunk, length);
        const uint8_t *buf = _work;
        const uint32_t n = _carryLen + length;

        // If we are mid-video-data, the carried bytes (if any) are unflushed data.
        if (_state == State::VideoData) _videoPendingStart = 0;

        bool headerPending = false;
        uint32_t markerPos = 0;
        uint32_t pos = 0;

        // Consume audio payload bytes first — they may have carried over from the previous chunk.
        consumeAudio(buf, n, pos);

        // Scan for markers (never inside an opaque audio payload).
        while (_state != State::AudioData && pos + 4 <= n) {
            if (!(buf[pos] == 0x00 && buf[pos + 1] == 0xFF && buf[pos + 2] == 0xFF)) {
                pos++;
                continue;
            }
            int32_t next = processMarker(buf, n, pos);
            if (next < 0) { // marker complete but header incomplete: carry it over
                headerPending = true;
                markerPos = pos;
                break;
            }
            pos = static_cast<uint32_t>(next);

            // An audio marker found mid-chunk: consume its payload before scanning further.
            consumeAudio(buf, n, pos);
        }

        // Carry the tail bytes that could start (or complete) a marker in the next chunk.
        _carryLen = 0;
        if (headerPending) {
            // Video data before the pending marker is final: flush it now so it is not lost.
            if (_state == State::VideoData) flushVideo(_videoPendingStart, markerPos);
            uint32_t tail = n - markerPos; // complete marker + partial header
            if (tail > MAX_CARRY) tail = MAX_CARRY;
            memcpy(_carry, buf + markerPos, tail);
            _carryLen = tail;
        } else if (_state != State::AudioData) {
            if (n >= 1 && buf[n - 1] == 0x00) _carryLen = 1;
            else if (n >= 2 && buf[n - 2] == 0x00 && buf[n - 1] == 0xFF) _carryLen = 2;
            else if (n >= 3 && buf[n - 3] == 0x00 && buf[n - 2] == 0xFF && buf[n - 1] == 0xFF) _carryLen = 3;

            // Flush all video data except the carried tail, which may prove to be a marker prefix.
            if (_state == State::VideoData) flushVideo(_videoPendingStart, n - _carryLen);
        }
    }

}
