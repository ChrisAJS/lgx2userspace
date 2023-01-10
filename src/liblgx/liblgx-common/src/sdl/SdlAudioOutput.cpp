#include <stdexcept>
#include "SdlAudioOutput.h"

namespace sdl {

    SdlAudioOutput::SdlAudioOutput() {
        if (SDL_Init(SDL_INIT_AUDIO) != 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void SdlAudioOutput::initialiseAudio() {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = 48000;
        want.format = AUDIO_S16LSB;
        want.channels = 2;
        want.samples = 1024;
        want.callback = nullptr;
        _audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        SDL_PauseAudioDevice(_audio, 0);
    }

    void SdlAudioOutput::audioFrameAvailable(uint32_t *audio) {
        SDL_QueueAudio(_audio, audio, 800 * 4);
    }

    void SdlAudioOutput::render() {
        // Should be where audio data is written to the audio pipe
    }

    void SdlAudioOutput::shutdownAudio() {
        SDL_PauseAudioDevice(_audio, 1);
        SDL_CloseAudioDevice(_audio);
    }
}
