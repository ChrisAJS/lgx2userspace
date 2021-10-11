#ifndef LGX2USERSPACE_SDLFRAMEOUTPUT_H
#define LGX2USERSPACE_SDLFRAMEOUTPUT_H


#include "lgxdevice.h"
#include <SDL.h>

namespace sdl {

    class SdlFrameOutput : public lgx2::VideoOutput, public lgx2::AudioOutput {
    public:
        SdlFrameOutput();

        void initialiseVideo() override;

        void initialiseAudio() override;

        void videoFrameAvailable(uint32_t *image) override;

        void audioFrameAvailable(uint32_t *audio) override;

        void display() override;

        void render() override;

    private:

        SDL_Renderer *_renderer;
        SDL_Texture *_texture;
        SDL_AudioDeviceID _audio;
    };
}


#endif //LGX2USERSPACE_SDLFRAMEOUTPUT_H
