#ifndef LGX2USERSPACE_SDLAUDIOOUTPUT_H
#define LGX2USERSPACE_SDLAUDIOOUTPUT_H

#include "lgxdevice.h"
#include <SDL3/SDL.h>

namespace sdl {
    class SdlAudioOutput : public lgx2::AudioOutput {
    public:
        SdlAudioOutput();

        void initialiseAudio() override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;

        void shutdownAudio() override;
    private:
        SDL_AudioStream *_stream{nullptr};
    };
}


#endif //LGX2USERSPACE_SDLAUDIOOUTPUT_H
