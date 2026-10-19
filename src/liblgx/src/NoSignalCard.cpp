#include "NoSignalCard.h"

#include <cstring>

namespace lgx2 {

    namespace {
        // 5x7 glyphs for just the characters the messages use; rows top to bottom, bit 4 = left.
        struct Glyph {
            char c;
            uint8_t rows[7];
        };

        constexpr Glyph GLYPHS[] = {
                {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
                {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
                {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
                {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
                {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
                {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
                {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
                {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
                {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
                {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
                {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
                {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
                {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
                {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
                {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
                {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
                {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
                {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
        };

        const uint8_t *glyphRows(char c) {
            for (const auto &glyph : GLYPHS) {
                if (glyph.c == c) return glyph.rows;
            }
            return nullptr; // space, or a character the messages do not use
        }

        // Limited-range YUY2: dark grey background, near-white text, neutral chroma.
        constexpr uint8_t BACKGROUND_Y = 0x20;
        constexpr uint8_t TEXT_Y = 0xEB;
        constexpr uint8_t NEUTRAL_C = 0x80;
        constexpr int SCALE = 12;            // screen pixels per font pixel
        constexpr int CELL = 6 * SCALE;      // glyph width plus one column of spacing
    }

    const char *NoSignalCard::message(InputState reason) {
        switch (reason) {
            case InputState::NoCable:
                return "NO HDMI CABLE";
            case InputState::NoDevice:
                return "NO CAPTURE DEVICE";
            case InputState::Protected:
                return "HDCP PROTECTED";
            default:
                return "NO SIGNAL";
        }
    }

    uint32_t *NoSignalCard::frame(InputState reason) {
        if (reason == InputState::Signal) reason = InputState::NoSignal;
        if (_frame.empty() || reason != _rendered) {
            render(message(reason));
            _rendered = reason;
        }
        return reinterpret_cast<uint32_t *>(_frame.data());
    }

    void NoSignalCard::render(const char *text) {
        _frame.resize(static_cast<size_t>(WIDTH) * HEIGHT * 2);
        for (size_t i = 0; i < _frame.size(); i += 2) {
            _frame[i] = BACKGROUND_Y;
            _frame[i + 1] = NEUTRAL_C;
        }

        const int length = static_cast<int>(strlen(text));
        const int left = (WIDTH - length * CELL + SCALE) / 2;
        const int top = (HEIGHT - 7 * SCALE) / 2;
        for (int i = 0; i < length; i++) {
            const uint8_t *rows = glyphRows(text[i]);
            if (rows == nullptr) continue;
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 5; col++) {
                    if (!(rows[row] & (0x10 >> col))) continue;
                    for (int y = 0; y < SCALE; y++) {
                        uint8_t *line = _frame.data() + static_cast<size_t>(top + row * SCALE + y) * WIDTH * 2;
                        for (int x = 0; x < SCALE; x++) {
                            // Only luma changes, so chroma pairing does not matter.
                            line[(left + i * CELL + col * SCALE + x) * 2] = TEXT_Y;
                        }
                    }
                }
            }
        }
    }
}
