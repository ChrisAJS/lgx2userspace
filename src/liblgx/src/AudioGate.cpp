#include <cstring>
#include "AudioGate.h"

namespace utils {

    void AudioGate::update(bool signalPresent, Clock::time_point now) {
        if (!signalPresent) {
            _present = false;
            _open = false;
            return;
        }
        if (!_present) {
            _present = true;
            _presentSince = now;
        }
        if (!_open && now - _presentSince >= SETTLE_TIME) _open = true;
    }

    bool AudioGate::open() const {
        return _open;
    }

    void AudioGate::process(uint8_t *pcm, uint32_t byteLength) {
        const uint32_t target = _open ? RAMP_FRAMES : 0;
        const uint32_t frames = byteLength / (CHANNELS * 2);

        if (_rampPosition == target) {
            // Steady state: pass through untouched, or silence while closed.
            if (target == 0) memset(pcm, 0, byteLength);
            return;
        }

        for (uint32_t f = 0; f < frames; f++) {
            if (_rampPosition < target) _rampPosition++;
            else if (_rampPosition > target) _rampPosition--;
            for (uint32_t c = 0; c < CHANNELS; c++) {
                uint8_t *s = pcm + (f * CHANNELS + c) * 2;
                auto sample = static_cast<int16_t>(s[0] | (s[1] << 8));
                auto scaled = static_cast<int16_t>(static_cast<int32_t>(sample) *
                                                   static_cast<int32_t>(_rampPosition) /
                                                   static_cast<int32_t>(RAMP_FRAMES));
                s[0] = static_cast<uint8_t>(scaled & 0xFF);
                s[1] = static_cast<uint8_t>((scaled >> 8) & 0xFF);
            }
        }
    }
}
