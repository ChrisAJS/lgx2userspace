#ifndef LGX2USERSPACE_AOAUDIOOUTPUT_H
#define LGX2USERSPACE_AOAUDIOOUTPUT_H

#include <lgxdevice.h>
#include <ao/ao.h>

namespace ao {
    class AoAudioOutput : public lgx2::AudioOutput {
    public:
        void initialiseAudio() override;

        void audioFrameAvailable(const uint8_t *audio, uint32_t byteLength) override;

        void render() override;

        void shutdownAudio() override;
    private:
        ao_device *_device;
    };
}


#endif //LGX2USERSPACE_AOAUDIOOUTPUT_H
