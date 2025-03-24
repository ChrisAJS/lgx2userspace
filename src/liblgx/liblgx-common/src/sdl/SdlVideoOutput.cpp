#include "SdlVideoOutput.h"

#include <SDL3/SDL.h>
#include <stdexcept>

namespace sdl {
    SdlVideoOutput::SdlVideoOutput() {
        if (SDL_Init(SDL_INIT_VIDEO) != true) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void SdlVideoOutput::initialiseVideo(lgx2::VideoScale scale) {
        _targetScale = scale;
        int width = 1920;
        int height = 1080;

        if (scale == lgx2::VideoScale::Half) {
            width = 1920 / 2;
            height = 1080 / 2;
        } else if (scale == lgx2::VideoScale::Quarter) {
            width = 1920 / 4;
            height = 1080 / 4;
        }

        if (!SDL_CreateWindowAndRenderer("lgx2userspace", width, height, 0,
                                    &_window, &_renderer)) {
            throw std::runtime_error(SDL_GetError());
        }

        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
        SDL_RenderClear(_renderer);
        SDL_RenderPresent(_renderer);

        _texture = SDL_CreateTexture(
            _renderer,
            SDL_PIXELFORMAT_YUY2,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height);
    }


    void SdlVideoOutput::videoFrameAvailable(uint32_t *image) {
        uint32_t *pixels{nullptr};
        int32_t pitch;
        bool result = SDL_LockTexture(_texture, nullptr, (void **) &pixels, &pitch);
        if (pixels != nullptr && result) {
            int pitchInInts = pitch / 4;

            if (_targetScale == lgx2::VideoScale::Full) {
                memcpy(pixels, image, 1920 * 1080 * 2);
            } else if (_targetScale == lgx2::VideoScale::Half) {
                for (int y = 0; y < 540; y++) {
                    for (int x = 0; x < 960; x++) {
                        int frameOffset = ((y * 960) + x) * 2;
                        int textureOffset = ((y * pitchInInts) + x);
                        pixels[textureOffset] = image[frameOffset];
                    }
                }
            } else {
                for (int y = 0; y < 1080; y += 4) {
                    for (int x = 0; x < 1920; x += 4) {
                        int frameOffset = y * 1920 + x;
                        int textureOffset = (((y >> 2) * (pitch >> 2)) + (x >> 2));
                        pixels[textureOffset] = image[frameOffset];
                    }
                }
            }
        }

        SDL_UnlockTexture(_texture);
    }

    void SdlVideoOutput::display() {
        const bool *keyboardState = SDL_GetKeyboardState(nullptr);

        if (keyboardState[SDL_SCANCODE_F]) {
            SDL_SetWindowFullscreen(_window, 1);
        } else if (keyboardState[SDL_SCANCODE_G]) {
            SDL_SetWindowFullscreen(_window, 0);
        }

        SDL_RenderTexture(_renderer, _texture, nullptr, nullptr);
        SDL_RenderPresent(_renderer);
    }

    void SdlVideoOutput::shutdownVideo() {
        SDL_DestroyTexture(_texture);
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
    }
}
