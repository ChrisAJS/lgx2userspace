#ifndef LGX2USERSPACE_BOOTSTRAPPER_H
#define LGX2USERSPACE_BOOTSTRAPPER_H

#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"
#include "CommandBridge.h"

namespace libusb {
    // Brings an opened, claimed board to a streaming-ready state: reuses a configuration left by
    // an earlier session when the board still holds one, otherwise replays the recorded bootstrap,
    // then applies the fixes the replay needs (see PROTOCOL.md).
    class Bootstrapper {
    public:
        Bootstrapper(libusb_device_handle *dev, CommandBridge &bridge);

        // Returns true when the board was already configured and the replay was skipped (the
        // stream will then not start on a frame boundary). Throws if the board cannot be used.
        bool bootstrap(lgx2::DeviceType deviceType);

    private:
        // Whether the board still holds a configuration from an earlier bootstrap (which
        // survives a USB reset), read back from registers the bootstrap sets.
        enum class BoardState { Fresh, Configured, Unknown };

        libusb_device_handle *_dev;
        CommandBridge &_bridge;

        BoardState detectLgx2BoardState();
        bool videoAlreadyFlowing();
        void setAutoInputColourSpace();
    };
}

#endif //LGX2USERSPACE_BOOTSTRAPPER_H
