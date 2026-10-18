#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <memory>
#include <vector>
#include "lgxdevice.h"
#include "UsbContext.h"
#include "DeviceSession.h"

namespace libusb {

    // lgx2::Stream over a USB-connected LGX2/LGX for the standalone viewer: finds the card at
    // startup and runs one DeviceSession on it. A session that ends (disconnect, failed stream)
    // is reported as an error from update(), so the viewer stops.
    class UsbStream : public lgx2::Stream {
    public:
        explicit UsbStream();
        ~UsbStream() override;

        bool deviceAvailable(lgx2::DeviceType deviceType) override;
        void streamSetupCommands(lgx2::DeviceType deviceType) override;
        void queueFrameRead(std::function<void(uint8_t *, uint32_t, bool)> *onData) override;
        void update() override;

        void shutdownStream() override;

        uint64_t droppedChunks() const override;
        uint64_t transferErrors() const override;

        // From the HDMI status poll; Signal when the poll is disabled or has given up.
        lgx2::InputState inputState() const override;

    private:
        // Declared first so it is destroyed last: the session's transfers need its event thread.
        UsbContext _context;
        std::vector<FoundDevice> _devices;
        std::unique_ptr<DeviceSession> _session;

        std::function<void(uint8_t *, uint32_t, bool)> *_onFrameDataCallback{nullptr};
    };
}

#endif //LGX2USERSPACE_USBSTREAM_H
