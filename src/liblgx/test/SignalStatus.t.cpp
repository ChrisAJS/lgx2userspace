#include "catch_amalgamated.hpp"

#include "SignalStatus.h"

using lgx2::AdvStatusRegisters;
using lgx2::SignalStatus;

namespace {
    void put16(uint8_t *p, uint16_t value) {
        p[0] = static_cast<uint8_t>(value >> 8);
        p[1] = static_cast<uint8_t>(value);
    }

    // Register snapshot for a locked CEA 1080p60 HDMI source (148.5 MHz, 2200x1125 total).
    AdvStatusRegisters locked1080p60() {
        AdvStatusRegisters r;
        r.io6f = 0x10;               // +5V on port A
        r.io6a = 0xF0;               // TMDS clock detected + PLL locked
        r.cpB1 = 0x80;               // STDI valid
        r.io02 = 0xFC;               // input colour space automatic, YCbCr output
        r.io60 = 0x01;               // AVI InfoFrame received
        r.avi01 = 0x00;              // colour format RGB
        r.hdmi05to0c[0] = 0x80;      // HDMI mode, no pixel repetition
        r.hdmi05to0c[1] = 148;       // TMDS MHz
        put16(&r.hdmi05to0c[2], 1920);
        put16(&r.hdmi05to0c[4], 1080);
        r.hdmi3b = 0x20;             // + 2 x 0.25 MHz
        put16(&r.hdmi20to25[0], 88); // H front porch
        put16(&r.hdmi20to25[2], 44); // H sync
        put16(&r.hdmi20to25[4], 148);// H back porch
        put16(&r.hdmi2ato35[0], 4 * 2);  // V front porch (half-lines)
        put16(&r.hdmi2ato35[4], 5 * 2);  // V sync
        put16(&r.hdmi2ato35[8], 36 * 2); // V back porch
        return r;
    }
}

TEST_CASE("Locked 1080p60 decodes to its CEA timing") {
    SignalStatus s = SignalStatus::decode(locked1080p60());

    CHECK(s.cableDetected);
    CHECK(s.signalPresent());
    CHECK(s.hdmiMode);
    CHECK_FALSE(s.interlaced);
    CHECK(s.width == 1920);
    CHECK(s.height == 1080);
    CHECK(s.totalWidth == 2200);
    CHECK(s.totalHeight == 1125);
    CHECK(s.pixelClockHz == 148500000);
    CHECK(s.refreshHz() == Catch::Approx(60.0));
    CHECK(s.colourFormat == SignalStatus::ColourFormat::RGB);
    CHECK(s.describe() == "1920x1080p @ 60.00 Hz (148.50 MHz, HDMI, RGB; receiver input colour space auto)");
}

TEST_CASE("AVI InfoFrame colour format and a fixed receiver input colour space are reported") {
    AdvStatusRegisters r = locked1080p60();
    r.avi01 = 0x20;   // Y1Y0 = 01: YCbCr 4:2:2
    r.io02 = 0x1C;    // input fixed to full-range RGB, as the replayed bootstrap leaves it

    SignalStatus s = SignalStatus::decode(r);
    CHECK(s.colourFormat == SignalStatus::ColourFormat::YCbCr422);
    CHECK(s.inputColourSpace == 0x1);
    CHECK(s.describe() == "1920x1080p @ 60.00 Hz (148.50 MHz, HDMI, YCbCr 4:2:2; receiver input colour space fixed 0x1)");

    // Without a received AVI InfoFrame the format is unknown.
    r.io60 = 0x00;
    CHECK(SignalStatus::decode(r).colourFormat == SignalStatus::ColourFormat::Unknown);

    // A change of source format is reported as a change.
    AdvStatusRegisters rgb = locked1080p60();
    CHECK(SignalStatus::decode(rgb) == SignalStatus::decode(locked1080p60()));
    rgb.avi01 = 0x40; // YCbCr 4:4:4
    CHECK(SignalStatus::decode(rgb) != SignalStatus::decode(locked1080p60()));
}

TEST_CASE("No +5V reports no cable") {
    AdvStatusRegisters r;
    SignalStatus s = SignalStatus::decode(r);
    CHECK_FALSE(s.cableDetected);
    CHECK_FALSE(s.signalPresent());
    CHECK(s.describe() == "no cable");
}

TEST_CASE("Cable without lock reports no signal") {
    AdvStatusRegisters r = locked1080p60();

    SECTION("TMDS PLL not locked") { r.io6a = 0x10; }
    SECTION("No TMDS clock") { r.io6a = 0xE0; }
    SECTION("STDI not valid") { r.cpB1 = 0x00; }
    SECTION("Chip powered down") { r.io0c = 0x24; }

    SignalStatus s = SignalStatus::decode(r);
    CHECK(s.cableDetected);
    CHECK_FALSE(s.signalPresent());
    CHECK(s.describe() == "cable connected, no signal");
}

TEST_CASE("Deep colour and pixel repetition divide the TMDS clock") {
    AdvStatusRegisters r = locked1080p60();
    r.hdmi05to0c[1] = 185;           // 185.625 MHz TMDS = 148.5 MHz at 10 bpc
    r.hdmi3b = 0x20;                 // + 0.5 MHz -> 185.5 (register resolution is 0.25 MHz)
    r.hdmi05to0c[6] = 0x20;          // 10 bits per channel

    SignalStatus s = SignalStatus::decode(r);
    CHECK(s.pixelClockHz == 148400000);

    r = locked1080p60();
    r.hdmi05to0c[0] = 0x81;          // pixel repetition x2
    s = SignalStatus::decode(r);
    CHECK(s.pixelClockHz == 74250000);
}

TEST_CASE("Interlaced height adds field 1 and ignores the deep colour bits") {
    AdvStatusRegisters r = locked1080p60();
    r.io12 = 0x10;                   // STDI interlaced
    put16(&r.hdmi05to0c[4], 540);    // field 0
    put16(&r.hdmi05to0c[6], 540);    // field 1 (0x0B low nibble + 0x0C)
    r.hdmi05to0c[6] |= 0x20;         // 10 bpc in the same register must not leak into the height

    SignalStatus s = SignalStatus::decode(r);
    CHECK(s.interlaced);
    CHECK(s.height == 1080);
    CHECK(s.describe().rfind("1920x1080i", 0) == 0);
}

TEST_CASE("Equality ignores timing noise without a signal and sub-register clock jitter") {
    AdvStatusRegisters a;
    AdvStatusRegisters b;
    a.io6f = b.io6f = 0x10;
    put16(&b.hdmi05to0c[2], 1234);   // garbage timing while unlocked
    CHECK(SignalStatus::decode(a) == SignalStatus::decode(b));

    AdvStatusRegisters locked = locked1080p60();
    CHECK(SignalStatus::decode(locked) != SignalStatus::decode(a));

    AdvStatusRegisters resized = locked1080p60();
    put16(&resized.hdmi05to0c[2], 1280);
    CHECK(SignalStatus::decode(locked) != SignalStatus::decode(resized));
}
