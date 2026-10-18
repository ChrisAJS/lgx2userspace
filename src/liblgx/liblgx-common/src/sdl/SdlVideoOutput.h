#ifndef LGX2USERSPACE_SDLVIDEOOUTPUT_H
#define LGX2USERSPACE_SDLVIDEOOUTPUT_H

#include "lgxdevice.h"
#include <SDL3/SDL.h>

namespace sdl {

    class SdlVideoOutput : public lgx2::VideoOutput {
    public:
        SdlVideoOutput();

        void initialiseVideo(lgx2::VideoScale scale) override;

        void videoFrameAvailable(uint32_t *image) override;

        void display() override;

        void shutdownVideo() override;

        void showNoSignal(lgx2::InputState reason) override;

    private:
        bool _noSignal{false}; // show the no-signal screen until the next frame arrives
        lgx2::InputState _noSignalReason{lgx2::InputState::NoSignal};

        void drawNoSignal();
        void drawHdmiPlug(float centreX, float centreY, float width, uint8_t brightness, bool crossedOut);
        void drawCentredText(const char *text, float centreY, float scale);
        SDL_Window *_window{};
        SDL_Renderer *_renderer{};
        SDL_Texture *_texture{};
        lgx2::VideoScale _targetScale{lgx2::VideoScale::Full};
    };
}


#endif //LGX2USERSPACE_SDLVIDEOOUTPUT_H
