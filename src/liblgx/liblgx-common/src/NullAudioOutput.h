#ifndef LGX2USERSPACE_NULLAUDIOOUTPUT_H
#define LGX2USERSPACE_NULLAUDIOOUTPUT_H

#include "lgxdevice.h"

class NullAudioOutput : public lgx2::AudioOutput {
public:
    void initialiseAudio() override;

    void audioFrameAvailable(uint32_t *audio) override;

    void render() override;

    void shutdownAudio() override;
};


#endif //LGX2USERSPACE_NULLAUDIOOUTPUT_H
