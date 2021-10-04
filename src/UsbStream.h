#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <vector>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"

namespace libusb {
    class UsbStream : public lgx2::Stream {
    public:
        UsbStream();

        void streamSetupCommands() override;
        void queueFrameRead(std::function<void(uint8_t *, uint32_t)> *onData) override;
        void update() override;

        void onFrameData(libusb_transfer *transfer);

        void queueFrameOne();
        void queueFrameTwo();

        void queueFutureFrame(libusb_transfer *transfer);

    private:
        libusb_device_handle *_dev;

        std::vector<libusb_transfer*> _transfers;

        std::function<void(uint8_t*, uint32_t)> *_onFrameDataCallback;

        uint8_t *_frameBuffer;
    };
}


#endif //LGX2USERSPACE_USBSTREAM_H
