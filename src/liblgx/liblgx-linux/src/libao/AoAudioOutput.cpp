#include "AoAudioOutput.h"

#include <ao/ao.h>

namespace ao {

    void AoAudioOutput::initialiseAudio() {
        ao_initialize();

        ao_sample_format format{
                .bits = 16,
                .rate = 48000,
                .channels = 2,
                .byte_format = AO_FMT_LITTLE,
                .matrix = "L,R"
        };

        _device = ao_open_live(0, &format, nullptr);
    }

    void AoAudioOutput::audioFrameAvailable(uint32_t *audio) {
        ao_play(_device, (char *) audio, 800 * 4);
    }

    void AoAudioOutput::render() {

    }

    void AoAudioOutput::shutdownAudio() {
        ao_close(_device);
        ao_shutdown();
    }
}