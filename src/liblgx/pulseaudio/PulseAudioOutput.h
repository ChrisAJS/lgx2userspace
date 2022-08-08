#ifndef LGX2USERSPACE_PULSEAUDIOOUTPUT_H
#define LGX2USERSPACE_PULSEAUDIOOUTPUT_H

#include "../lgxdevice.h"
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

namespace pulse {

class PulseAudioOutput : public lgx2::AudioOutput {
    public:
    explicit PulseAudioOutput(std::string pulseAudioSink);

    void initialiseAudio() override;

    void audioFrameAvailable(uint32_t *audio) override;

    void render() override;

    void shutdownAudio() override;

private:
    std::string _pulseAudioSink;
    pa_simple *_pulseAudioHandle;
};

}


#endif //LGX2USERSPACE_PULSEAUDIOOUTPUT_H
