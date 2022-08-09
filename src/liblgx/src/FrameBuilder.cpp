#include <cstring>
#include "FrameBuilder.h"

namespace utils {

    inline uint32_t min(uint32_t a, uint32_t b) {
        return (a < b) ? a : b;
    }

    FrameBuilder::FrameBuilder() : _videoFrame{new uint32_t[1920*1080]}, _audioFrame{new uint32_t[800]},
                                   _videoOffset{0}, _audioOffset{0} {

    }

    void FrameBuilder::buildVideo(uint8_t *block, uint32_t len) {
        uint32_t blocksUntilCompleteFrame = 1920 * 1080 - _videoOffset;
        uint32_t blocksCopied = min(len, blocksUntilCompleteFrame);

        memcpy(_videoFrame + _videoOffset, block, blocksCopied * 4);
        _videoOffset += blocksCopied;
    }

    void FrameBuilder::buildAudio(uint8_t *block, uint32_t len) {
        uint32_t blocksUntilCompleteFrame = 800 - _audioOffset;
        uint32_t blocksCopied = min(len, blocksUntilCompleteFrame);

        memcpy(_audioFrame + _audioOffset, block, blocksCopied * 4);
        _audioOffset += blocksCopied;
    }

    uint32_t *FrameBuilder::completeVideoFrame() {
        _videoOffset = 0;
        return _videoFrame;
    }

    uint32_t *FrameBuilder::completeAudioFrame() {
        _audioOffset = 0;
        return _audioFrame;
    }

    uint32_t FrameBuilder::videoFrameSize() {
        return _videoOffset;
    }

}