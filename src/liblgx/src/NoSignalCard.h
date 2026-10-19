#ifndef LGX2USERSPACE_NOSIGNALCARD_H
#define LGX2USERSPACE_NOSIGNALCARD_H

#include <cstdint>
#include <vector>
#include "lgxinterfaces.h"

namespace lgx2 {
    // A 1920x1080 YUY2 frame saying why there is no picture ("NO SIGNAL", "NO HDMI CABLE",
    // "NO CAPTURE DEVICE", "HDCP PROTECTED"), for outputs that need real frames rather than drawing their own
    // screen. Rendered on first use per reason and cached.
    class NoSignalCard {
    public:
        static constexpr int WIDTH = 1920;
        static constexpr int HEIGHT = 1080;

        // The card for `reason` (Signal is treated as NoSignal). Valid until the next call.
        uint32_t *frame(InputState reason);

        static const char *message(InputState reason);

    private:
        InputState _rendered{InputState::Signal};
        std::vector<uint8_t> _frame;

        void render(const char *text);
    };
}

#endif //LGX2USERSPACE_NOSIGNALCARD_H
