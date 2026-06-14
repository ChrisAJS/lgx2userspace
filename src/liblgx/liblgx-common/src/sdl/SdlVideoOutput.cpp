#include "SdlVideoOutput.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace sdl {

    SdlVideoOutput::SdlVideoOutput() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
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

        _window = SDL_CreateWindow("lgx2userspace", width, height, SDL_WINDOW_RESIZABLE);
        _renderer = SDL_CreateRenderer(_window, nullptr);
        // No vsync: the compositor would throttle SDL_RenderPresent for occluded windows,
        // starving the USB dequeue loop. The USB transfer rate (~60fps) provides natural pacing.

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
        if (_targetScale == lgx2::VideoScale::Full) {
            // YUY2: 2 bytes per pixel, pitch = width * 2
            if (!SDL_UpdateTexture(_texture, nullptr, image, 1920 * 2)) {
                fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
            }
            return;
        }

        // Scaled modes: downsample into a temporary buffer then upload.
        // YUY2 macropixel = 1 uint32 covers 2 horizontal pixels, so
        // half-width = 480 uint32s, quarter-width = 240 uint32s.
        int texW = (_targetScale == lgx2::VideoScale::Half) ? 960  : 480;
        int texH = (_targetScale == lgx2::VideoScale::Half) ? 540  : 270;
        int step = (_targetScale == lgx2::VideoScale::Half) ? 2    : 4;

        std::vector<uint32_t> buf(texW * texH);
        for (int y = 0; y < texH; y++) {
            for (int x = 0; x < texW; x++) {
                // Sample one macropixel from the source at the scaled position
                buf[y * texW + x] = image[(y * step) * 960 + x * step];
            }
        }
        if (!SDL_UpdateTexture(_texture, nullptr, buf.data(), texW * 2)) {
            fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        }
    }

    void SdlVideoOutput::display() {
        const bool *keyboardState = SDL_GetKeyboardState(nullptr);

        if (keyboardState[SDL_SCANCODE_F]) {
            SDL_SetWindowFullscreen(_window, true);
        } else if (keyboardState[SDL_SCANCODE_G]) {
            SDL_SetWindowFullscreen(_window, false);
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
