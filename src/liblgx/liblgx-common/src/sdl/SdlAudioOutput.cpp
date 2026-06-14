#include <stdexcept>
#include "SdlAudioOutput.h"

namespace sdl {

    SdlAudioOutput::SdlAudioOutput() {
        if (!SDL_Init(SDL_INIT_AUDIO)) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void SdlAudioOutput::initialiseAudio() {
        SDL_AudioSpec spec{};
        spec.freq     = 48000;
        spec.format   = SDL_AUDIO_S16LE;
        spec.channels = 2;

        _stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!_stream) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(_stream));
    }

    void SdlAudioOutput::audioFrameAvailable(uint32_t *audio) {
    }

    void SdlAudioOutput::render() {
    }

    void SdlAudioOutput::shutdownAudio() {
        SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(_stream));
        SDL_DestroyAudioStream(_stream);
    }
}
