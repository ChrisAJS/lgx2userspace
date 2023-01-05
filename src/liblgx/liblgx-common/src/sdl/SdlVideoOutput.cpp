#include "SdlVideoOutput.h"

#include <SDL2/SDL.h>
#include <stdexcept>

namespace sdl {

    SdlVideoOutput::SdlVideoOutput() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void SdlVideoOutput::initialiseVideo() {
        _window = SDL_CreateWindow("lgx2userspace",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   1920, 1080,
                                   SDL_WINDOW_RESIZABLE);
        _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
        SDL_RenderClear(_renderer);
        SDL_RenderPresent(_renderer);

        _texture = SDL_CreateTexture(
                _renderer,
                SDL_PIXELFORMAT_YUY2,
                SDL_TEXTUREACCESS_STREAMING,
                1920,
                1080);
    }


    void SdlVideoOutput::videoFrameAvailable(uint32_t *image) {
        uint8_t *pixels{nullptr};
        int32_t pitch;
        int result = SDL_LockTexture(_texture, nullptr, (void **) &pixels, &pitch);
        if (pixels != nullptr && result == 0) {
            memcpy(pixels, image, 1920 * 1080 * 2);
        }
        SDL_UnlockTexture(_texture);
    }

    void SdlVideoOutput::display() {
        const Uint8 *keyboardState = SDL_GetKeyboardState(nullptr);

        if (keyboardState[SDL_SCANCODE_F]) {
            SDL_SetWindowFullscreen(_window, 1);
        } else if (keyboardState[SDL_SCANCODE_G]) {
            SDL_SetWindowFullscreen(_window, 0);
        }

        SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
        SDL_RenderPresent(_renderer);
    }

    void SdlVideoOutput::shutdownVideo() {
        SDL_DestroyTexture(_texture);
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
    }
}