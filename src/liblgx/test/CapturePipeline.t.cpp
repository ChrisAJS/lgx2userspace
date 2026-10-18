#include "catch_amalgamated.hpp"

#include "CapturePipeline.h"

#include <vector>

using lgx2::CapturePipeline;
using lgx2::InputState;
using lgx2::NoSignalCard;
using namespace std::chrono_literals;

namespace {
    constexpr uint32_t FRAME_BYTES = utils::FrameBuilder::VIDEO_FRAME_BYTES;
    constexpr uint32_t AUDIO_BYTES = 3200;
    constexpr uint32_t BUF = 0x4000;

    uint8_t frameByte(uint32_t i, uint8_t fill) {
        return static_cast<uint8_t>(0x10 + ((i ^ fill) % 0xD0)); // never 00 or FF: no false markers
    }

    void padToBuffer(std::vector<uint8_t> &out) {
        while (out.size() % 4) out.push_back(0x00);
        while (out.size() % BUF) out.insert(out.end(), {0xAA, 0x55, 0x55, 0xAA});
    }

    // One frame period as the hardware lays it out (normal buffer order): SAV, frame, EAV padded
    // to 12, audio packet, filler to a 16 KiB buffer boundary.
    void appendPeriod(std::vector<uint8_t> &out, uint8_t b0, uint8_t fill, int16_t audioLevel) {
        const uint8_t s1 = 0x01, s2 = 0x81;
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0xC0, b0, s1, s2, static_cast<uint8_t>(b0 + s1 + s2 - 0x40)});
        for (uint32_t i = 0; i < FRAME_BYTES; i++) out.push_back(frameByte(i, fill));
        const uint8_t c1 = 0x02, c2 = 0x04, c3 = 0x38;
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0xC1, b0, c1, c2, c3, static_cast<uint8_t>(b0 + c1 + c2 + c3 - 0x3F)});
        out.insert(out.end(), 3, 0x00);
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0x58, 0x00, 0x00, 0x0C, 0x80});
        for (uint32_t i = 0; i < AUDIO_BYTES / 2; i++) {
            out.push_back(static_cast<uint8_t>(audioLevel & 0xFF));
            out.push_back(static_cast<uint8_t>((audioLevel >> 8) & 0xFF));
        }
        padToBuffer(out);
    }

    // `frames` periods plus trailing filler, so everything clears the reorderer's hold-back.
    std::vector<uint8_t> stream(int frames) {
        std::vector<uint8_t> out;
        for (int f = 0; f < frames; f++) appendPeriod(out, static_cast<uint8_t>(0x10 + f), static_cast<uint8_t>(f * 0x21), 1000);
        for (int i = 0; i < 8; i++) {
            out.insert(out.end(), BUF, 0);
            for (size_t j = out.size() - BUF; j < out.size(); j += 4) {
                out[j] = 0xAA; out[j + 1] = 0x55; out[j + 2] = 0x55; out[j + 3] = 0xAA;
            }
        }
        return out;
    }

    void feedAll(CapturePipeline &pipeline, std::vector<uint8_t> &data) {
        for (size_t offset = 0; offset < data.size(); offset += 0x1FC000) {
            uint32_t length = static_cast<uint32_t>(std::min<size_t>(0x1FC000, data.size() - offset));
            pipeline.feed(data.data() + offset, length, false);
        }
    }

    struct FakeVideo : lgx2::VideoOutput {
        std::vector<std::vector<uint8_t>> frames;
        std::vector<InputState> noSignalCalls;

        void initialiseVideo(lgx2::VideoScale) override {}
        void videoFrameAvailable(uint32_t *image) override {
            auto *bytes = reinterpret_cast<uint8_t *>(image);
            frames.emplace_back(bytes, bytes + FRAME_BYTES);
        }
        void display() override {}
        void shutdownVideo() override {}
        void showNoSignal(InputState reason) override { noSignalCalls.push_back(reason); }
    };

    struct FakeAudio : lgx2::AudioOutput {
        std::vector<std::vector<uint8_t>> packets;

        void initialiseAudio() override {}
        void audioFrameAvailable(const uint8_t *audio, uint32_t byteLength) override {
            packets.emplace_back(audio, audio + byteLength);
        }
        void render() override {}
        void shutdownAudio() override {}
    };

    bool allZero(const std::vector<uint8_t> &bytes) {
        for (uint8_t b : bytes) if (b != 0) return false;
        return true;
    }
}

TEST_CASE("With a settled signal, frames and audio reach the outputs") {
    FakeVideo video;
    FakeAudio audio;
    CapturePipeline pipeline{&video, &audio, CapturePipeline::Options{}};
    CapturePipeline::Clock::time_point t{};
    pipeline.setInputState(InputState::Signal, t);
    pipeline.setInputState(InputState::Signal, t + 600ms); // past the audio gate's settle time

    auto data = stream(3);
    feedAll(pipeline, data);

    REQUIRE(video.frames.size() == 3);
    REQUIRE(pipeline.videoFramesEmitted() == 3);
    for (uint32_t i = 0; i < FRAME_BYTES; i += 997) {
        REQUIRE(video.frames[1][i] == frameByte(i, 0x21));
    }
    REQUIRE(audio.packets.size() == 3);
    REQUIRE(audio.packets[2].size() == AUDIO_BYTES);
    CHECK(audio.packets[2][0] == (1000 & 0xFF)); // fully open by the third packet
    CHECK(video.noSignalCalls.empty());
}

TEST_CASE("Without a signal, frames are withheld, the output is told why, and audio is silent") {
    FakeVideo video;
    FakeAudio audio;
    CapturePipeline pipeline{&video, &audio, CapturePipeline::Options{}};
    CapturePipeline::Clock::time_point t{};

    pipeline.setInputState(InputState::NoCable, t);
    pipeline.setInputState(InputState::NoCable, t + 50ms); // repeated: not reported again
    REQUIRE(video.noSignalCalls == std::vector<InputState>{InputState::NoCable});

    auto data = stream(2);
    feedAll(pipeline, data);
    CHECK(video.frames.empty());
    CHECK(pipeline.videoFramesEmitted() == 0);
    REQUIRE(audio.packets.size() == 2);
    CHECK(allZero(audio.packets[0]));
    CHECK(allZero(audio.packets[1]));

    pipeline.setInputState(InputState::NoSignal, t + 100ms); // reason changed: reported
    CHECK(video.noSignalCalls == std::vector<InputState>{InputState::NoCable, InputState::NoSignal});
}

TEST_CASE("Keep-alive frames carry the no-signal card at the configured rate") {
    FakeVideo video;
    CapturePipeline::Options options;
    options.keepAliveFrames = true;
    options.keepAliveInterval = 33ms;
    CapturePipeline pipeline{&video, nullptr, options};
    CapturePipeline::Clock::time_point t{};

    pipeline.setInputState(InputState::NoCable, t);
    pipeline.tick(t);
    pipeline.tick(t + 10ms);   // too soon
    pipeline.tick(t + 40ms);
    REQUIRE(video.frames.size() == 2);
    CHECK(pipeline.keepAliveFramesEmitted() == 2);

    NoSignalCard card;
    auto *expected = reinterpret_cast<uint8_t *>(card.frame(InputState::NoCable));
    CHECK(std::equal(video.frames[0].begin(), video.frames[0].end(), expected));

    // A new reason is shown straight away, without waiting for the interval.
    pipeline.setInputState(InputState::NoDevice, t + 45ms);
    pipeline.tick(t + 46ms);
    REQUIRE(video.frames.size() == 3);
    expected = reinterpret_cast<uint8_t *>(card.frame(InputState::NoDevice));
    CHECK(std::equal(video.frames[2].begin(), video.frames[2].end(), expected));

    // With a signal, the captured frames are the output: no keep-alive.
    pipeline.setInputState(InputState::Signal, t + 100ms);
    pipeline.tick(t + 200ms);
    CHECK(video.frames.size() == 3);
}

TEST_CASE("Keep-alive frames are off by default") {
    FakeVideo video;
    CapturePipeline pipeline{&video, nullptr, CapturePipeline::Options{}};
    pipeline.setInputState(InputState::NoSignal, CapturePipeline::Clock::time_point{});
    pipeline.tick(CapturePipeline::Clock::time_point{} + 1s);
    CHECK(video.frames.empty());
}

TEST_CASE("The no-signal card is a 1080p YUY2 frame with the message drawn in") {
    NoSignalCard card;
    auto *bytes = reinterpret_cast<uint8_t *>(card.frame(InputState::NoSignal));
    const size_t size = static_cast<size_t>(NoSignalCard::WIDTH) * NoSignalCard::HEIGHT * 2;

    size_t textPixels = 0;
    for (size_t i = 0; i < size; i += 2) {
        CHECK((bytes[i] == 0x20 || bytes[i] == 0xEB));
        if (bytes[i] == 0xEB) textPixels++;
        if (i % 4 == 1) CHECK(bytes[i] == 0x80);
    }
    CHECK(textPixels > 10000);
    // Top-left corner is background.
    CHECK(bytes[0] == 0x20);

    std::vector<uint8_t> noSignal(bytes, bytes + size);
    auto *other = reinterpret_cast<uint8_t *>(card.frame(InputState::NoCable));
    CHECK_FALSE(std::equal(noSignal.begin(), noSignal.end(), other));
    CHECK(std::string(NoSignalCard::message(InputState::NoDevice)) == "NO CAPTURE DEVICE");
}
