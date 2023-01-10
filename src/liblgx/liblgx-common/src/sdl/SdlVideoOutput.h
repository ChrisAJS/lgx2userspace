#ifndef LGX2USERSPACE_SDLVIDEOOUTPUT_H
#define LGX2USERSPACE_SDLVIDEOOUTPUT_H

#include "lgxdevice.h"
#include <SDL2/SDL.h>

namespace sdl {

    class SdlVideoOutput : public lgx2::VideoOutput {
    public:
        SdlVideoOutput();

        void initialiseVideo(lgx2::VideoScale scale) override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

    private:
        SDL_Window *_window{};
        SDL_Renderer *_renderer{};
        SDL_Texture *_texture{};
        lgx2::VideoScale _targetScale{lgx2::VideoScale::Full};
    };
}


#endif //LGX2USERSPACE_SDLVIDEOOUTPUT_H
