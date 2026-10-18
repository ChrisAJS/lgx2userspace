#include "catch_amalgamated.hpp"

#include "AudioGate.h"

#include <cstdlib>
#include <vector>

using utils::AudioGate;
using namespace std::chrono_literals;

namespace {
    // One packet of constant-amplitude stereo samples (1/60 s at 48 kHz = 800 frames).
    std::vector<uint8_t> packet(int16_t value, uint32_t frames = 800) {
        std::vector<uint8_t> out;
        for (uint32_t i = 0; i < frames * AudioGate::CHANNELS; i++) {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }
        return out;
    }

    int16_t sampleAt(const std::vector<uint8_t> &pcm, uint32_t frame, uint32_t channel = 0) {
        size_t i = (frame * AudioGate::CHANNELS + channel) * 2;
        return static_cast<int16_t>(pcm[i] | (pcm[i + 1] << 8));
    }

    bool allZero(const std::vector<uint8_t> &pcm) {
        for (uint8_t b : pcm) if (b != 0) return false;
        return true;
    }
}

TEST_CASE("Gate starts closed and outputs silence") {
    AudioGate gate;
    auto pcm = packet(10000);
    gate.update(false, AudioGate::Clock::time_point{});
    gate.process(pcm.data(), static_cast<uint32_t>(pcm.size()));
    CHECK_FALSE(gate.open());
    CHECK(allZero(pcm));
}

TEST_CASE("Gate opens only after the signal has settled, with a ramp") {
    AudioGate gate;
    AudioGate::Clock::time_point t{};

    gate.update(true, t);
    auto early = packet(10000);
    gate.process(early.data(), static_cast<uint32_t>(early.size()));
    CHECK_FALSE(gate.open());
    CHECK(allZero(early));

    gate.update(true, t + AudioGate::SETTLE_TIME);
    REQUIRE(gate.open());
    auto ramp = packet(10000);
    gate.process(ramp.data(), static_cast<uint32_t>(ramp.size()));
    // Rises gradually from near zero, never jumping straight to full volume.
    CHECK(std::abs(sampleAt(ramp, 0)) < 100);
    for (uint32_t f = 1; f < AudioGate::RAMP_FRAMES; f++) {
        CHECK(sampleAt(ramp, f) >= sampleAt(ramp, f - 1));
    }
    CHECK(sampleAt(ramp, AudioGate::RAMP_FRAMES) == 10000);
    CHECK(sampleAt(ramp, 799, 1) == 10000);

    // Fully open: later packets pass through untouched.
    auto steady = packet(-12345);
    auto copy = steady;
    gate.update(true, t + AudioGate::SETTLE_TIME + 20ms);
    gate.process(steady.data(), static_cast<uint32_t>(steady.size()));
    CHECK(steady == copy);
}

TEST_CASE("Signal loss closes the gate immediately with a ramp down, then silence") {
    AudioGate gate;
    AudioGate::Clock::time_point t{};
    gate.update(true, t);
    gate.update(true, t + AudioGate::SETTLE_TIME);
    auto warmup = packet(10000);
    gate.process(warmup.data(), static_cast<uint32_t>(warmup.size()));

    gate.update(false, t + AudioGate::SETTLE_TIME + 10ms);
    CHECK_FALSE(gate.open());
    auto fade = packet(10000);
    gate.process(fade.data(), static_cast<uint32_t>(fade.size()));
    CHECK(sampleAt(fade, 0) > 9900);
    for (uint32_t f = 1; f < AudioGate::RAMP_FRAMES; f++) {
        CHECK(sampleAt(fade, f) <= sampleAt(fade, f - 1));
    }
    CHECK(sampleAt(fade, AudioGate::RAMP_FRAMES) == 0);

    auto garbage = packet(32000);
    gate.process(garbage.data(), static_cast<uint32_t>(garbage.size()));
    CHECK(allZero(garbage));
}

TEST_CASE("A signal that flickers back for less than the settle time stays muted") {
    AudioGate gate;
    AudioGate::Clock::time_point t{};
    gate.update(true, t);
    gate.update(true, t + 400ms);
    gate.update(false, t + 450ms);  // drops out before settling
    gate.update(true, t + 500ms);   // settle timer restarts here
    gate.update(true, t + 900ms);
    CHECK_FALSE(gate.open());
    gate.update(true, t + 1000ms);
    CHECK(gate.open());
}
