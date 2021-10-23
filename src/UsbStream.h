#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <vector>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"

namespace libusb {

    enum class LGXDeviceType {
        LGX,
        LGX2
    };


    class UsbStream : public lgx2::Stream {
    public:
        UsbStream(LGXDeviceType targetDevice);

        void streamSetupCommands() override;
        void queueFrameRead(std::function<void(uint8_t *)> *onData) override;
        void update() override;

        void onFrameData(libusb_transfer *transfer);

        void shutdownStream() override;

        void submitTransfer(libusb_transfer *transfer);

    private:
        LGXDeviceType _deviceType;

        libusb_device_handle *_dev;

        std::vector<libusb_transfer*> _transfers;

        std::function<void(uint8_t*)> *_onFrameDataCallback;

        uint8_t *_frameBuffer;
        bool _shuttingDown;
    };
}


#endif //LGX2USERSPACE_USBSTREAM_H
