#ifndef LGX2USERSPACE_SIGNALSTATUS_H
#define LGX2USERSPACE_SIGNALSTATUS_H

#include <cstdint>
#include <string>

namespace lgx2 {
    // Raw ADV7604 register snapshot taken by the periodic status poll. This is a small subset
    // of what the Windows driver's 200 ms CRXADV7604 poll reads (see PROTOCOL.md, "Runtime
    // polling"); register meanings and the signal test follow Linux drivers/media/i2c/adv7604.c
    // (no_signal(), adv76xx_query_dv_timings(), adv76xx_read_hdmi_pixelclock()).
    struct AdvStatusRegisters {
        // IO map (I2C 0x20)
        uint8_t io0c{0};   // bits 5,2 = power-down
        uint8_t io12{0};   // bit 4 = STDI says interlaced
        uint8_t io6a{0};   // bit 4 = TMDS clock detected (port A), bits 7:5 = TMDS PLL lock
        uint8_t io6f{0};   // bit 4 = +5V cable detect (port A)

        // CP map (I2C 0x22)
        uint8_t cpB1{0};   // bit 7 = STDI data valid

        // HDMI map (I2C 0x34), read as blocks
        uint8_t hdmi05to0c[8]{};   // 05 mode/polarity/pixel-rep, 06 TMDS MHz, 07-08 width,
                                   // 09-0A field0 height, 0B deep colour + 0B-0C field1 height
        uint8_t hdmi20to25[6]{};   // H front porch, H sync, H back porch
        uint8_t hdmi2ato35[12]{};  // V front porch, V sync, V back porch (field0/field1, half-lines)
        uint8_t hdmi3b{0};         // bits 5:4 = TMDS frequency fraction (quarter MHz)
    };

    struct SignalStatus {
        bool cableDetected{false};
        bool tmdsClockDetected{false};
        bool tmdsLocked{false};
        bool stdiValid{false};
        bool poweredDown{false};
        bool hdmiMode{false};    // false = DVI
        bool interlaced{false};

        uint16_t width{0};
        uint16_t height{0};      // per frame (both fields when interlaced)
        uint16_t totalWidth{0};
        uint16_t totalHeight{0}; // field 0 only when interlaced
        uint32_t pixelClockHz{0};

        static SignalStatus decode(const AdvStatusRegisters &regs);

        // Mirrors Linux adv76xx no_signal() for HDMI port A on an ADV7604.
        bool signalPresent() const;

        // Frame rate (field rate when interlaced); 0 when unknown.
        double refreshHz() const;

        // e.g. "1920x1080p @ 60.00 Hz (148.50 MHz, HDMI)", "no cable", "cable connected, no signal"
        std::string describe() const;

        bool operator==(const SignalStatus &other) const;
        bool operator!=(const SignalStatus &other) const { return !(*this == other); }
    };
}

#endif //LGX2USERSPACE_SIGNALSTATUS_H
