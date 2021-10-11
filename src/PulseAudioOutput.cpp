#include <stdexcept>
#include "PulseAudioOutput.h"

void pulse::PulseAudioOutput::initialiseAudio() {

    static const pa_sample_spec sampleSpec = {
            .format = PA_SAMPLE_S16LE,
            .rate = 48000,
            .channels = 2
    };

    /* Create a new playback stream */
    _pulseAudioHandle = pa_simple_new(nullptr, "LGX2 Userspace driver", PA_STREAM_PLAYBACK, "lgx2", "capture audio", &sampleSpec, nullptr, nullptr, nullptr);
}

void pulse::PulseAudioOutput::audioFrameAvailable(uint32_t *audio) {
    if (pa_simple_write(_pulseAudioHandle, audio, 800*4, nullptr) < 0) {
        throw std::runtime_error("Failed to write audio data to pulseaudio\n");
    }
}

void pulse::PulseAudioOutput::render() {

}
