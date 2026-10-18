#include "catch_amalgamated.hpp"

#include "BufferReorderer.h"
#include "FrameBuilder.h"

#include <vector>

using utils::BufferReorderer;
using utils::FrameBuilder;

namespace {
    constexpr uint32_t BUF = BufferReorderer::DMA_BUFFER_BYTES;

    void appendSav(std::vector<uint8_t> &out, uint8_t b0) {
        const uint8_t b1 = 0x01, b2 = 0x81;
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0xC0, b0, b1, b2, static_cast<uint8_t>(b0 + b1 + b2 - 0x40)});
    }

    void appendEav(std::vector<uint8_t> &out, uint8_t c0) {
        const uint8_t c1 = 0x02, c2 = 0x04, c3 = 0x38;
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0xC1, c0, c1, c2, c3,
                               static_cast<uint8_t>(c0 + c1 + c2 + c3 - 0x3F)});
    }

    uint8_t frameByte(uint32_t i, uint8_t fill) {
        return static_cast<uint8_t>(0x10 + ((i ^ fill) % 0xD0)); // never 00 or FF: no false markers
    }

    void padToBuffer(std::vector<uint8_t> &out) {
        while (out.size() % 4) out.push_back(0x00);
        while (out.size() % BUF) out.insert(out.end(), {0xAA, 0x55, 0x55, 0xAA});
    }

    // End block: EAV padded to 12, a 3200-byte audio packet, AA5555AA filler to a buffer boundary.
    // fakeSav puts a checksum-valid SAV pattern inside the audio data (audio is arbitrary bytes).
    void appendEndBlock(std::vector<uint8_t> &out, uint8_t c0, bool fakeSav = false) {
        appendEav(out, c0);
        out.insert(out.end(), 3, 0x00);
        out.insert(out.end(), {0x00, 0xFF, 0xFF, 0x58, 0x00, 0x00, 0x0C, 0x80});
        size_t audioStart = out.size();
        for (uint32_t i = 0; i < 3200; i++) out.push_back(static_cast<uint8_t>(i * 7));
        if (fakeSav) {
            std::vector<uint8_t> sav;
            appendSav(sav, 0x42);
            std::copy(sav.begin(), sav.end(), out.begin() + static_cast<std::ptrdiff_t>(audioStart + 100));
        }
        padToBuffer(out);
    }

    // One frame period in true order: SAV starts a DMA buffer, then the frame and its end block.
    // The period must be an even number of buffers so SAVs keep their place in the buffer pair, as
    // on hardware at 1080p (254 buffers).
    void appendPeriod(std::vector<uint8_t> &out, uint8_t b0, uint32_t frameBytes, uint8_t fill, bool fakeSav) {
        size_t start = out.size();
        appendSav(out, b0);
        for (uint32_t i = 0; i < frameBytes; i++) out.push_back(frameByte(i, fill));
        appendEndBlock(out, b0, fakeSav);
        REQUIRE(((out.size() - start) / BUF) % 2 == 0);
    }

    // Frame size giving an 8-buffer period with the end block in the last buffer.
    constexpr uint32_t SMALL_FRAME = 120000;

    // The device's frame counter: increments but skips 00 (… fe ff 01 02 …).
    uint8_t nextB0(uint8_t b0) {
        return b0 == 0xFF ? 0x01 : static_cast<uint8_t>(b0 + 1);
    }

    // True-order stream: the previous frame's end block fills buffer 0, so each SAV sits in the
    // second buffer of a pair, as observed on hardware, then `periods` frame periods.
    std::vector<uint8_t> trueStream(uint32_t periods, uint32_t frameBytes, uint8_t firstB0 = 0xD7,
                                    int fakeSavPeriod = -1) {
        std::vector<uint8_t> out;
        for (uint32_t i = 0; i < BUF - 3300; i++) out.push_back(frameByte(i, 0xFF)); // previous frame's tail
        appendEndBlock(out, static_cast<uint8_t>(firstB0 - 1));
        uint8_t b0 = firstB0;
        for (uint32_t p = 0; p < periods; p++, b0 = nextB0(b0)) {
            appendPeriod(out, b0, frameBytes, static_cast<uint8_t>(p * 0x21), static_cast<int>(p) == fakeSavPeriod);
        }
        return out;
    }

    // What the device delivers in the bad state: every 32 KiB pair swapped.
    std::vector<uint8_t> swapPairs(const std::vector<uint8_t> &in) {
        std::vector<uint8_t> out(in);
        for (size_t p = 0; p + 2 * BUF <= in.size(); p += 2 * BUF) {
            std::copy(in.begin() + p + BUF, in.begin() + p + 2 * BUF, out.begin() + p);
            std::copy(in.begin() + p, in.begin() + p + BUF, out.begin() + p + BUF);
        }
        return out;
    }

    struct Output {
        std::vector<uint8_t> bytes;
        std::vector<size_t> gapAt; // byte offsets in `bytes` where a gapped call started
    };

    void capture(BufferReorderer &reorderer, Output &output) {
        reorderer.setSink([&output](uint8_t *data, uint32_t length, bool gapped) {
            if (gapped) output.gapAt.push_back(output.bytes.size());
            output.bytes.insert(output.bytes.end(), data, data + length);
        });
    }

    void feedInChunks(BufferReorderer &reorderer, const std::vector<uint8_t> &stream, size_t from, size_t to,
                      uint32_t chunkSize, bool firstGapped = false) {
        for (size_t offset = from; offset < to; offset += chunkSize) {
            uint32_t length = static_cast<uint32_t>(std::min<size_t>(chunkSize, to - offset));
            reorderer.feed(stream.data() + offset, length, firstGapped && offset == from);
        }
    }

    size_t findSav(const std::vector<uint8_t> &s, size_t from) {
        for (size_t i = from; i + 4 <= s.size(); i++) {
            if (s[i] == 0x00 && s[i + 1] == 0xFF && s[i + 2] == 0xFF && s[i + 3] == 0xC0) return i;
        }
        return SIZE_MAX;
    }

    bool matchesAt(const std::vector<uint8_t> &expected, size_t offset, const std::vector<uint8_t> &actual,
                   size_t actualFrom = 0) {
        size_t n = actual.size() - actualFrom;
        return offset + n <= expected.size() &&
               std::equal(actual.begin() + actualFrom, actual.end(), expected.begin() + offset);
    }
}

TEST_CASE("Normal order passes through from the first SAV", "[bufferreorderer]") {
    auto stream = trueStream(4, SMALL_FRAME);
    uint32_t chunk = GENERATE(4093u, 0x1FC000u);

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);
    feedInChunks(reorderer, stream, 0, stream.size(), chunk);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Normal);
    REQUIRE(reorderer.orderChanges() == 1);
    REQUIRE(output.gapAt == std::vector<size_t>{0});
    REQUIRE(output.bytes.size() > stream.size() / 2);
    REQUIRE(matchesAt(stream, findSav(stream, 0), output.bytes));
}

TEST_CASE("Swapped buffer pairs are restored to true order", "[bufferreorderer]") {
    auto truth = trueStream(4, SMALL_FRAME);
    auto swapped = swapPairs(truth);
    uint32_t chunk = GENERATE(4093u, 0x1FC000u);

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);
    feedInChunks(reorderer, swapped, 0, swapped.size(), chunk);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Swapped);
    REQUIRE(reorderer.orderChanges() == 1);
    REQUIRE(output.gapAt == std::vector<size_t>{0});
    // Anchored at the first SAV of the swapped stream (offset 0): output is the true stream.
    REQUIRE(findSav(swapped, 0) == 0);
    REQUIRE(output.bytes.size() > truth.size() / 2);
    REQUIRE(matchesAt(truth, 0, output.bytes));
}

TEST_CASE("A gap re-detects the order and re-anchors the pair grid", "[bufferreorderer]") {
    auto truth = trueStream(6, SMALL_FRAME);
    auto swapped = swapPairs(truth);

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);

    // Feed a third, then lose an arbitrary, non-pair-aligned run of bytes.
    size_t cut = swapped.size() / 3;
    size_t resume = cut + 3 * BUF + 1000;
    feedInChunks(reorderer, swapped, 0, cut, 0x1FC000);
    size_t beforeGap = output.bytes.size();
    feedInChunks(reorderer, swapped, resume, swapped.size(), 0x1FC000, true);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Swapped);
    REQUIRE(output.gapAt.size() == 2);
    REQUIRE(output.gapAt[1] == beforeGap);
    // After the gap, output restarts at the next SAV in the swapped stream, in true order.
    size_t nextSav = findSav(swapped, resume);
    REQUIRE(matchesAt(truth, nextSav, output.bytes, beforeGap));
}

TEST_CASE("A change of order mid-stream is detected", "[bufferreorderer]") {
    // Normal for a few frames, then the device's pairs swap (e.g. it dropped a buffer).
    auto first = trueStream(3, SMALL_FRAME, 0x10);
    auto secondTruth = trueStream(3, SMALL_FRAME, 0x13);
    auto second = swapPairs(secondTruth);
    std::vector<uint8_t> stream(first);
    stream.insert(stream.end(), second.begin(), second.end());

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);
    feedInChunks(reorderer, stream, 0, stream.size(), 0x1FC000);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Swapped);
    REQUIRE(reorderer.orderChanges() == 2);
    REQUIRE(output.gapAt.size() == 2);
    // The switch takes effect on the second swapped SAV, anchored there.
    size_t secondSwappedSav = findSav(second, findSav(second, 0) + 4);
    REQUIRE(matchesAt(secondTruth, secondSwappedSav, output.bytes, output.gapAt[1]));
}

TEST_CASE("Frame counter wrap (fe ff 01, no 00) is still recognised as swapped", "[bufferreorderer]") {
    auto truth = trueStream(5, SMALL_FRAME, 0xFD);
    auto swapped = swapPairs(truth);

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);
    feedInChunks(reorderer, swapped, 0, swapped.size(), 0x1FC000);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Swapped);
    REQUIRE(reorderer.orderChanges() == 1);
    REQUIRE(matchesAt(truth, 0, output.bytes));
}

TEST_CASE("A SAV-like pattern inside audio data does not flip the order", "[bufferreorderer]") {
    auto truth = trueStream(5, SMALL_FRAME, 0xD7, 2);
    auto swapped = swapPairs(truth);

    BufferReorderer reorderer;
    Output output;
    capture(reorderer, output);
    feedInChunks(reorderer, swapped, 0, swapped.size(), 0x1FC000);

    REQUIRE(reorderer.order() == BufferReorderer::Order::Swapped);
    REQUIRE(reorderer.orderChanges() == 1);
    REQUIRE(output.gapAt.size() == 1);
    REQUIRE(matchesAt(truth, 0, output.bytes));
}

TEST_CASE("Swapped 1080p stream yields correct frames through the FrameBuilder", "[bufferreorderer]") {
    auto truth = trueStream(4, FrameBuilder::VIDEO_FRAME_BYTES);
    // 1080p periods are exactly 254 buffers with 11,108 bytes of filler, as on hardware.
    REQUIRE((truth.size() - BUF) / 4 == 254 * BUF);
    auto swapped = swapPairs(truth);

    BufferReorderer reorderer;
    FrameBuilder builder;
    std::vector<std::vector<uint8_t>> frames;
    reorderer.setSink([&builder](uint8_t *data, uint32_t length, bool gapped) { builder.feed(data, length, gapped); });
    builder.setVideoFrameCallback([&frames](const uint8_t *data, uint32_t length) {
        frames.emplace_back(data, data + length);
    });
    feedInChunks(reorderer, swapped, 0, swapped.size(), 0x1FC000);

    // The last frame's EAV is still in the held-back tail; the first three are complete.
    REQUIRE(frames.size() == 3);
    REQUIRE(builder.desyncCount() == 0);
    for (uint32_t f = 0; f < 3; f++) {
        for (uint32_t i = 0; i < FrameBuilder::VIDEO_FRAME_BYTES; i += 997) {
            REQUIRE(frames[f][i] == frameByte(i, static_cast<uint8_t>(f * 0x21)));
        }
    }
}
