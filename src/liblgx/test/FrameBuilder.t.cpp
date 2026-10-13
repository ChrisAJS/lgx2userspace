#include "catch_amalgamated.hpp"

#include "FrameBuilder.h"

#include <vector>

using utils::FrameBuilder;

namespace {
    // Valid SAV header: b3 == (b0 + b1 + b2 - 0x40) mod 256. (Bit 0 of b2 is *not*
    // required to be clear — verified against a live capture where it is usually set.)
    void appendSav(std::vector<uint8_t> &out, uint8_t b0 = 0x01) {
        const uint8_t b1 = 0x02;
        const uint8_t b2 = 0x04;
        out.push_back(0x00);
        out.push_back(0xFF);
        out.push_back(0xFF);
        out.push_back(0xC0);
        out.push_back(b0);
        out.push_back(b1);
        out.push_back(b2);
        out.push_back(static_cast<uint8_t>(b0 + b1 + b2 - 0x40));
    }

    // Valid EAV header: c4 == (c0 + c1 + c2 + c3 - 0x3F) mod 256.
    void appendEav(std::vector<uint8_t> &out, uint8_t c0 = 0x01) {
        const uint8_t c1 = 0x03;
        const uint8_t c2 = 0x05;
        const uint8_t c3 = 0x07;
        out.push_back(0x00);
        out.push_back(0xFF);
        out.push_back(0xFF);
        out.push_back(0xC1);
        out.push_back(c0);
        out.push_back(c1);
        out.push_back(c2);
        out.push_back(c3);
        out.push_back(static_cast<uint8_t>(c0 + c1 + c2 + c3 - 0x3F));
    }

    void appendAudioStart(std::vector<uint8_t> &out, uint32_t payloadLength) {
        out.push_back(0x00);
        out.push_back(0xFF);
        out.push_back(0xFF);
        out.push_back(0x58);
        out.push_back(static_cast<uint8_t>(payloadLength >> 24));
        out.push_back(static_cast<uint8_t>(payloadLength >> 16));
        out.push_back(static_cast<uint8_t>(payloadLength >> 8));
        out.push_back(static_cast<uint8_t>(payloadLength));
    }

    // Byte pattern for a frame's data, so content can be verified per byte.
    uint8_t frameByte(uint32_t i, uint8_t baseFill) {
        return static_cast<uint8_t>((i & 0xFF) ^ baseFill);
    }

    // One complete video frame, matching the real device layout: a single SAV, then exactly
    // VIDEO_FRAME_BYTES of data, then one EAV with c0 == b0 (a clean pair).
    void appendVideoFrame(std::vector<uint8_t> &out, uint8_t baseFill) {
        appendSav(out, 0x01);
        for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i++) out.push_back(frameByte(i, baseFill));
        appendEav(out, 0x01);
    }

    struct FrameCapture {
        // Frames are copied by value: the builder round-robins two internal buffers, so a
        // pointer into one is clobbered by the next frame within the same feed().
        std::vector<std::vector<uint8_t>> videoFrames;
        std::vector<uint32_t> videoFrameLengths;
        std::vector<std::vector<uint8_t>> audioFrames;
    };

    void captureCallbacks(FrameBuilder &builder, FrameCapture &capture) {
        builder.setVideoFrameCallback([&capture](const uint8_t *data, uint32_t length) {
            capture.videoFrames.emplace_back(data, data + length);
            capture.videoFrameLengths.push_back(length);
        });
        builder.setAudioFrameCallback([&capture](const uint8_t *data, uint32_t length) {
            capture.audioFrames.emplace_back(data, data + length);
        });
    }
}

TEST_CASE("Video frame is emitted from a complete SAV->EAV pair", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    std::vector<uint8_t> stream;
    appendVideoFrame(stream, 0x00);

    builder.feed(stream.data(), static_cast<uint32_t>(stream.size()), false);

    REQUIRE(builder.videoFramesEmitted() == 1);
    REQUIRE(capture.videoFrameLengths.size() == 1);
    REQUIRE(capture.videoFrameLengths[0] == FrameBuilder::VIDEO_FRAME_BYTES);
    REQUIRE(builder.desyncCount() == 0);

    const auto &frame = capture.videoFrames[0];
    for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i += 997) {
        REQUIRE(frame[i] == frameByte(i, 0x00));
    }
}

TEST_CASE("Video frame assembly with straddling chunk boundaries", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    std::vector<uint8_t> stream;
    appendVideoFrame(stream, 0x20);

    // Feed in awkwardly-sized chunks so markers and headers straddle boundaries. A single
    // frame spans multiple USB transfers, so this exercises carry reassembly too.
    const uint32_t chunkSize = 7;
    for (uint32_t offset = 0; offset < stream.size(); offset += chunkSize) {
        uint32_t length = std::min(chunkSize, static_cast<uint32_t>(stream.size() - offset));
        builder.feed(stream.data() + offset, length, false);
    }

    REQUIRE(builder.videoFramesEmitted() == 1);
    REQUIRE(capture.videoFrameLengths[0] == FrameBuilder::VIDEO_FRAME_BYTES);
    REQUIRE(builder.desyncCount() == 0);

    const auto &frame = capture.videoFrames[0];
    for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i += 997) {
        REQUIRE(frame[i] == frameByte(i, 0x20));
    }
}

TEST_CASE("Invalid SAV checksum is ignored without desync", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    std::vector<uint8_t> stream;
    stream.push_back(0xAB); // filler that is not a marker
    appendSav(stream);
    stream.back() = static_cast<uint8_t>(stream.back() + 1); // corrupt the checksum byte
    for (uint32_t i = 0; i < 1000; i++) stream.push_back(0xCD);
    appendEav(stream); // stray EAV (parser is idle): counted as nothing

    builder.feed(stream.data(), static_cast<uint32_t>(stream.size()), false);
    REQUIRE(builder.videoFramesEmitted() == 0);
    REQUIRE(builder.desyncCount() == 0);

    // The parser is still idle and accepts a clean stream afterwards.
    std::vector<uint8_t> good;
    appendVideoFrame(good, 0x40);
    builder.feed(good.data(), static_cast<uint32_t>(good.size()), false);
    REQUIRE(builder.videoFramesEmitted() == 1);
}

TEST_CASE("EAV b0/c0 mismatch increments desync but still emits the frame", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    std::vector<uint8_t> stream;
    appendSav(stream, 0x02); // SAV b0 = 0x02
    for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i++) stream.push_back(0x11);
    appendEav(stream, 0x01); // EAV c0 = 0x01: mismatch with b0

    builder.feed(stream.data(), static_cast<uint32_t>(stream.size()), false);
    REQUIRE(builder.desyncCount() == 1);
    REQUIRE(builder.videoFramesEmitted() == 1); // data is still a full frame, so it is shown
}

TEST_CASE("Startup partial span is not emitted", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    // A SAV->EAV pair whose data is far smaller than one frame (e.g. the first partial
    // chunk after boot) must not be shown as a frame.
    std::vector<uint8_t> stream;
    appendSav(stream);
    for (uint32_t i = 0; i < 4096; i++) stream.push_back(0x33);
    appendEav(stream);

    builder.feed(stream.data(), static_cast<uint32_t>(stream.size()), false);
    REQUIRE(builder.videoFramesEmitted() == 0);
    REQUIRE(builder.desyncCount() == 0);
}

TEST_CASE("Audio packet is length-declared and spans chunk boundaries", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    const uint32_t audioLength = 3000;
    std::vector<uint8_t> stream;
    appendSav(stream);
    for (uint32_t i = 0; i < 512; i++) stream.push_back(0x77); // small video chunk (not a frame)
    appendEav(stream);
    appendAudioStart(stream, audioLength);
    for (uint32_t i = 0; i < audioLength; i++) stream.push_back(static_cast<uint8_t>(i & 0xFF));

    // Split the stream so the audio payload spans two chunks.
    const uint32_t splitAt = 600; // inside the audio payload
    builder.feed(stream.data(), splitAt, false);
    REQUIRE(builder.audioFramesEmitted() == 0);
    builder.feed(stream.data() + splitAt, static_cast<uint32_t>(stream.size() - splitAt), false);

    REQUIRE(builder.audioFramesEmitted() == 1);
    REQUIRE(capture.audioFrames.size() == 1);
    REQUIRE(capture.audioFrames[0].size() == audioLength);
    for (uint32_t i = 0; i < audioLength; i += 97) {
        REQUIRE(capture.audioFrames[0][i] == static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE("Gap resets parser state and requires a clean pair", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    // A partial video chunk plus an audio packet cut short...
    std::vector<uint8_t> partial;
    appendSav(partial);
    for (uint32_t i = 0; i < 1000; i++) partial.push_back(0xAA);
    appendAudioStart(partial, 2048);
    for (uint32_t i = 0; i < 512; i++) partial.push_back(0x5A); // only 512 of 2048 audio bytes
    builder.feed(partial.data(), static_cast<uint32_t>(partial.size()), false);

    // ...then bytes are lost. The pre-gap partial audio must not be emitted.
    std::vector<uint8_t> garbage(64, 0x99);
    builder.feed(garbage.data(), static_cast<uint32_t>(garbage.size()), true);
    REQUIRE(builder.audioFramesEmitted() == 0);
    REQUIRE(builder.videoFramesEmitted() == 0);

    // A clean stream re-syncs and emits exactly one frame, with no pre-gap data spliced in.
    std::vector<uint8_t> good;
    appendVideoFrame(good, 0x80);
    builder.feed(good.data(), static_cast<uint32_t>(good.size()), false);

    REQUIRE(builder.videoFramesEmitted() == 1);
    const auto &frame = capture.videoFrames[0];
    for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i += 997) {
        REQUIRE(frame[i] == frameByte(i, 0x80));
    }
}

TEST_CASE("Implausible audio length is not treated as a marker", "[framebuilder]") {
    FrameBuilder builder;
    FrameCapture capture;
    captureCallbacks(builder, capture);

    std::vector<uint8_t> stream;
    appendAudioStart(stream, 0xFFFFFFFF); // garbage length header
    for (uint32_t i = 0; i < 100; i++) stream.push_back(0xEE);

    builder.feed(stream.data(), static_cast<uint32_t>(stream.size()), false);
    REQUIRE(builder.audioFramesEmitted() == 0);

    // Parsing continues normally afterwards.
    std::vector<uint8_t> good;
    appendVideoFrame(good, 0xA0);
    builder.feed(good.data(), static_cast<uint32_t>(good.size()), false);
    REQUIRE(builder.videoFramesEmitted() == 1);
}
