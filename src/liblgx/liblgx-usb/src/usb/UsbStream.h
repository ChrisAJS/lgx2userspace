#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <memory>
#include <vector>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"
#include "CommandBridge.h"
#include "Bootstrapper.h"
#include "StatusMonitor.h"
#include "CaptureStream.h"

namespace libusb {

    // lgx2::Stream over a USB-connected LGX2/LGX: finds and opens the device, then composes the
    // CommandBridge (register access), Bootstrapper (make it stream), StatusMonitor (HDMI input
    // state) and CaptureStream (the IN 0x83 data).
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
        libusb_device_handle *_dev{nullptr};
        lgx2::DeviceType _deviceType{lgx2::DeviceType::LGX2};
        std::vector<lgx2::DeviceType> _availableDevices;
        bool _alreadyBootstrapped{false};

        std::unique_ptr<CommandBridge> _bridge;
        std::unique_ptr<StatusMonitor> _statusMonitor;
        std::unique_ptr<CaptureStream> _capture;

        std::function<void(uint8_t *, uint32_t, bool)> *_onFrameDataCallback{nullptr};

        // The status poll reads ADV7604 registers, which only the GC551 has. Set
        // LGX2_NO_STATUS_POLL=1 to disable it.
        bool statusPollEnabled() const;
    };
}

#endif //LGX2USERSPACE_USBSTREAM_H
