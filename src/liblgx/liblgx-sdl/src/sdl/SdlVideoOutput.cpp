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


    void SdlVideoOutput::showNoSignal(lgx2::InputState reason) {
        _noSignal = true;
        _noSignalReason = reason;
    }

    void SdlVideoOutput::drawCentredText(const char *text, float centreY, float scale) {
        // SDL's built-in debug font: 8x8 pixels per character, drawn at the render scale.
        int width = 0, height = 0;
        SDL_GetCurrentRenderOutputSize(_renderer, &width, &height);
        const float glyph = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
        const float textWidth = static_cast<float>(SDL_strlen(text)) * glyph;
        // Never wider than 90% of the window.
        float fit = 0.9f * static_cast<float>(width) / textWidth;
        if (scale > fit) scale = fit;
        if (scale < 1.0f) scale = 1.0f;

        SDL_SetRenderScale(_renderer, scale, scale);
        SDL_RenderDebugText(_renderer, (static_cast<float>(width) / scale - textWidth) / 2.0f,
                            centreY / scale - glyph / 2.0f, text);
        SDL_SetRenderScale(_renderer, 1.0f, 1.0f);
    }

    void SdlVideoOutput::drawHdmiPlug(float centreX, float centreY, float width, uint8_t brightness,
                                      bool crossedOut) {
        // Outline of an HDMI type A plug face: wide top, chamfered lower corners.
        const float w = width / 2.0f;
        const float h = width * 0.36f;
        const SDL_FPoint shape[] = {
                {-w, -h / 2}, {w, -h / 2}, {w, h * 0.05f}, {w * 0.78f, h / 2},
                {-w * 0.78f, h / 2}, {-w, h * 0.05f}, {-w, -h / 2},
        };
        const SDL_FPoint pins[] = {
                {-w * 0.68f, -h * 0.18f}, {w * 0.68f, -h * 0.18f}, {w * 0.68f, h * 0.05f},
                {-w * 0.68f, h * 0.05f}, {-w * 0.68f, -h * 0.18f},
        };

        SDL_SetRenderDrawColor(_renderer, brightness, brightness, brightness, 255);
        const float thickness = width / 60.0f < 1.0f ? 1.0f : width / 60.0f;
        // Thicken by offsetting along each axis separately, so diagonal edges keep an even stroke.
        for (float o = -thickness / 2; o <= thickness / 2; o += 1.0f) {
            for (int axis = 0; axis < 2; axis++) {
                const float dx = axis == 0 ? o : 0.0f;
                const float dy = axis == 1 ? o : 0.0f;
                SDL_FPoint outline[7], inner[5];
                for (int i = 0; i < 7; i++) outline[i] = {centreX + shape[i].x + dx, centreY + shape[i].y + dy};
                for (int i = 0; i < 5; i++) inner[i] = {centreX + pins[i].x + dx, centreY + pins[i].y + dy};
                SDL_RenderLines(_renderer, outline, 7);
                SDL_RenderLines(_renderer, inner, 5);
            }
        }

        if (crossedOut) {
            SDL_SetRenderDrawColor(_renderer, 220, 70, 70, 255);
            for (float o = -thickness; o <= thickness; o += 1.0f) {
                SDL_RenderLine(_renderer, centreX - w * 1.1f, centreY + h + o, centreX + w * 1.1f, centreY - h + o);
            }
        }
    }

    void SdlVideoOutput::drawNoSignal() {
        int width = 0, height = 0;
        SDL_GetCurrentRenderOutputSize(_renderer, &width, &height);
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float unit = (w / 1920.0f < h / 1080.0f ? w / 1920.0f : h / 1080.0f); // 1.0 at 1080p

        SDL_SetRenderDrawColor(_renderer, 18, 18, 22, 255);
        SDL_RenderClear(_renderer);

        // A slow pulse so it is obvious the app is alive and waiting, not frozen.
        const float seconds = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        const float pulse = 0.5f + 0.5f * SDL_sinf(seconds * 2.5f);
        const auto brightness = static_cast<uint8_t>(110 + 90 * pulse);

        const bool noCable = _noSignalReason == lgx2::InputState::NoCable;
        const bool noDevice = _noSignalReason == lgx2::InputState::NoDevice;
        const bool isProtected = _noSignalReason == lgx2::InputState::Protected;
        drawHdmiPlug(w / 2.0f, h * 0.36f, 360.0f * unit, brightness, noCable || noDevice);

        SDL_SetRenderDrawColor(_renderer, 235, 235, 235, 255);
        drawCentredText(noDevice ? "NO CAPTURE DEVICE" : noCable ? "NO HDMI CABLE" : isProtected ? "HDCP PROTECTED" : "NO SIGNAL",
                        h * 0.58f, 9.0f * unit);

        SDL_SetRenderDrawColor(_renderer, 150, 150, 160, 255);
        if (isProtected) {
            drawCentredText("The source is sending copy-protected (HDCP) video, which cannot be captured", h * 0.68f, 3.0f * unit);
        } else if (noDevice) {
            drawCentredText("Connect the LGX2 to a USB 3 port", h * 0.68f, 3.0f * unit);
        } else if (noCable) {
            drawCentredText("Connect an HDMI source to the LGX2 input", h * 0.68f, 3.0f * unit);
        } else {
            static const char *waiting[] = {"Cable connected - waiting for the source   ",
                                            "Cable connected - waiting for the source.  ",
                                            "Cable connected - waiting for the source.. ",
                                            "Cable connected - waiting for the source..."};
            drawCentredText(waiting[static_cast<int>(seconds * 2.0f) % 4], h * 0.68f, 3.0f * unit);
        }

        // Restore the black draw colour the normal path clears with.
        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
    }

    void SdlVideoOutput::videoFrameAvailable(uint32_t *image) {
        _noSignal = false;
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

        if (_noSignal) {
            drawNoSignal();
        } else {
            // Draw colour is black (set at initialisation).
            SDL_RenderClear(_renderer);
            SDL_RenderTexture(_renderer, _texture, nullptr, nullptr);
        }
        SDL_RenderPresent(_renderer);
    }

    void SdlVideoOutput::shutdownVideo() {
        SDL_DestroyTexture(_texture);
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
    }
}
