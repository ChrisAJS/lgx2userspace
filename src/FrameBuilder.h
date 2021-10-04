#ifndef LGX2USERSPACE_FRAMEBUILDER_H
#define LGX2USERSPACE_FRAMEBUILDER_H

#include <stdint.h>

namespace utils {
    class FrameBuilder {
    public:
        FrameBuilder();
        uint8_t * buildVideo(uint8_t *block, uint32_t len);
        void buildAudio(uint8_t *block, uint32_t len);
        uint32_t *completeVideoFrame();
        uint32_t *completeAudioFrame();

        bool buildingVideoFrame();

        uint32_t blocksUntilVideoComplete();
    private:
        uint32_t *_videoFrame;
        uint32_t *_audioFrame;

        uint32_t _videoOffset;
        uint32_t _audioOffset;
    };
}


#endif //LGX2USERSPACE_FRAMEBUILDER_H
