#include <stdexcept>
#include "SdlAudioOutput.h"

namespace sdl {

    SdlAudioOutput::SdlAudioOutput() {
        if (!SDL_Init(SDL_INIT_AUDIO)) {
            throw std::runtime_error(SDL_GetError());
        }
    }

    void SdlAudioOutput::initialiseAudio() {
        SDL_AudioSpec want;
        SDL_zero(want);
        want.freq = 48000;
        want.format = SDL_AUDIO_S16LE;
        want.channels = 2;
        _audio = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want);
        SDL_ResumeAudioDevice(_audio);

        _stream = SDL_CreateAudioStream(&want, &want);
    }

    void SdlAudioOutput::audioFrameAvailable(uint32_t *audio) {
        SDL_PutAudioStreamData(_stream, audio, 800 * 4);
    }

    void SdlAudioOutput::render() {
        // SDL_FlushAudioStream(_stream);
    }

    void SdlAudioOutput::shutdownAudio() {
        SDL_PauseAudioDevice(_audio);
        SDL_CloseAudioDevice(_audio);
    }
}
