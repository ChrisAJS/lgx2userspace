#include <cstring>
#include <cstdio>
#include "FrameBuilder.h"

namespace utils {

    FrameBuilder::FrameBuilder() : _videoFrame{0}, _audioFrame{0},
                                   _videoOffset{0}, _audioOffset{0} {

    }

    uint8_t *FrameBuilder::buildVideo(uint8_t *block, uint32_t len) {
        uint32_t blocksUntilCompleteFrame = 1920 * 1080 - _videoOffset;
        uint32_t blocksCopied = (len > blocksUntilCompleteFrame) ? blocksUntilCompleteFrame : len;

        memcpy(_videoFrame + _videoOffset, block, blocksCopied * 4);
        _videoOffset += blocksCopied;
        return block + blocksCopied * 4;
    }

    void FrameBuilder::buildAudio(uint8_t *block, uint32_t len) {
        uint32_t blocksUntilCompleteFrame = 800 - _audioOffset;
        uint32_t blocksCopied = (len > blocksUntilCompleteFrame) ? blocksUntilCompleteFrame : len;

        memcpy(_audioFrame + _audioOffset, block, blocksCopied * 4);
        _audioOffset += blocksCopied;
    }

    uint32_t *FrameBuilder::completeVideoFrame() {
        if (_videoOffset == 1920 * 1080) {
            _videoOffset = 0;
            return _videoFrame;
        } else {
            _videoOffset = 0;
        }
        return _videoFrame;
    }

    uint32_t *FrameBuilder::completeAudioFrame() {
        if (_audioOffset == 800) {
            _audioOffset = 0;
            return _audioFrame;
        } else {
            _audioOffset = 0;
        }
        return _audioFrame;
    }

}