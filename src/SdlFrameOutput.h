#ifndef LGX2USERSPACE_SDLFRAMEOUTPUT_H
#define LGX2USERSPACE_SDLFRAMEOUTPUT_H


#include "lgxdevice.h"
#include <SDL.h>

namespace sdl {

    class SdlFrameOutput : public lgx2::FrameOutput {
    public:
        SdlFrameOutput();

        void videoFrameAvailable(uint32_t *image) override;

        void audioFrameAvailable(uint32_t *audio) override;

        void render() override;

    private:

        SDL_Renderer *_renderer;
        SDL_Texture *_texture;
        SDL_AudioDeviceID _audio;
    };
}


#endif //LGX2USERSPACE_SDLFRAMEOUTPUT_H
