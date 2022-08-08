#ifndef LGX2USERSPACE_FRAMEBUILDER_H
#define LGX2USERSPACE_FRAMEBUILDER_H

#include <cstdint>

namespace utils {
    class FrameBuilder {
    public:
        FrameBuilder();
        void buildVideo(uint8_t *block, uint32_t len);
        void buildAudio(uint8_t *block, uint32_t len);
        uint32_t *completeVideoFrame();
        uint32_t *completeAudioFrame();

        uint32_t videoFrameSize();

        const static uint32_t VIDEO_FRAME_START_MARKER = 0xC0FFFF00;
        const static uint32_t VIDEO_FRAME_END_MARKER   = 0xC1FFFF00;
        const static uint32_t AUDIO_FRAME_START_MARKER = 0xAA5555AA;

    private:
        uint32_t _videoFrame[1920 * 1080];
        uint32_t _audioFrame[800];

        uint32_t _videoOffset;
        uint32_t _audioOffset;
    };
}


#endif //LGX2USERSPACE_FRAMEBUILDER_H
