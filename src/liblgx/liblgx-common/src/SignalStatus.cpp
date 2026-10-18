#include "SignalStatus.h"

#include <cstdio>

namespace lgx2 {
    namespace {
        uint16_t read16(const uint8_t *p, uint16_t mask) {
            return static_cast<uint16_t>(((p[0] << 8) | p[1]) & mask);
        }
    }

    SignalStatus SignalStatus::decode(const AdvStatusRegisters &regs) {
        SignalStatus s;
        const uint8_t *h = regs.hdmi05to0c;   // h[n] = HDMI register 0x05 + n

        s.cableDetected = regs.io6f & 0x10;
        s.tmdsClockDetected = regs.io6a & 0x10;
        s.tmdsLocked = (regs.io6a & 0xE0) == 0xE0;
        s.stdiValid = regs.cpB1 & 0x80;
        s.poweredDown = regs.io0c & 0x24;
        s.hdmiMode = h[0] & 0x80;
        s.interlaced = regs.io12 & 0x10;

        s.width = read16(&h[2], 0x0FFF);
        s.height = read16(&h[4], 0x0FFF);
        if (s.interlaced) {
            s.height = static_cast<uint16_t>(s.height + read16(&h[6], 0x0FFF));
        }

        const uint8_t *hp = regs.hdmi20to25;
        s.totalWidth = static_cast<uint16_t>(read16(&hp[0], 0x3FF) + read16(&hp[2], 0x3FF) +
                                             read16(&hp[4], 0x3FF) + s.width);

        // Vertical porches are reported in half-lines; [0]=0x2A vfp0, [4]=0x2E vsync0, [8]=0x32 vbp0.
        const uint8_t *vp = regs.hdmi2ato35;
        uint16_t field0Height = read16(&h[4], 0x0FFF);
        s.totalHeight = static_cast<uint16_t>(read16(&vp[0], 0x1FFF) / 2 + read16(&vp[4], 0x1FFF) / 2 +
                                              read16(&vp[8], 0x1FFF) / 2 + field0Height);

        uint32_t tmdsHz = h[1] * 1000000u + ((regs.hdmi3b & 0x30) >> 4) * 250000u;
        if (s.hdmiMode) {
            // Deep colour and pixel repetition run the TMDS clock faster than the pixel clock.
            uint32_t bitsPerChannel = ((h[6] & 0x60) >> 4) + 8;
            uint32_t pixelRepetition = (h[0] & 0x0F) + 1;
            tmdsHz = static_cast<uint32_t>(static_cast<uint64_t>(tmdsHz) * 8 / bitsPerChannel / pixelRepetition);
        }
        s.pixelClockHz = tmdsHz;

        return s;
    }

    bool SignalStatus::signalPresent() const {
        // SSPD is not checked: Linux skips it on AFE-equipped parts like the ADV7604.
        return !poweredDown && stdiValid && tmdsLocked && tmdsClockDetected;
    }

    double SignalStatus::refreshHz() const {
        if (totalWidth == 0 || totalHeight == 0) return 0.0;
        return static_cast<double>(pixelClockHz) / (static_cast<double>(totalWidth) * totalHeight);
    }

    std::string SignalStatus::describe() const {
        if (!cableDetected) return "no cable";
        if (!signalPresent()) return "cable connected, no signal";

        char text[96];
        snprintf(text, sizeof(text), "%ux%u%c @ %.2f Hz (%.2f MHz, %s)",
                 width, height, interlaced ? 'i' : 'p', refreshHz(),
                 pixelClockHz / 1e6, hdmiMode ? "HDMI" : "DVI");
        return text;
    }

    bool SignalStatus::operator==(const SignalStatus &other) const {
        // Equality means "would be reported the same": timing registers are meaningless without
        // a signal, and the pixel clock is compared to 0.25 MHz (the register's resolution), so
        // measurement noise is not reported as a change.
        if (cableDetected != other.cableDetected || signalPresent() != other.signalPresent()) return false;
        if (!signalPresent()) return true;
        return hdmiMode == other.hdmiMode &&
               interlaced == other.interlaced &&
               width == other.width &&
               height == other.height &&
               totalWidth == other.totalWidth &&
               totalHeight == other.totalHeight &&
               pixelClockHz / 250000 == other.pixelClockHz / 250000;
    }
}
