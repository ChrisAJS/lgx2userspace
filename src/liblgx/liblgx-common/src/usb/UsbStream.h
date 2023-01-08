#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <vector>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"

namespace libusb {

    class UsbStream : public lgx2::Stream {
    public:
        explicit UsbStream();
        ~UsbStream() override;

        bool deviceAvailable(lgx2::DeviceType deviceType) override;
        void streamSetupCommands(lgx2::DeviceType deviceType) override;
        void queueFrameRead(std::function<void(uint8_t *)> *onData) override;
        void update() override;

        void onFrameData(libusb_transfer *transfer);

        void shutdownStream() override;

        void submitTransfer(libusb_transfer *transfer);

        void queueAllFrameReads();

    private:
        libusb_device_handle *_dev;

        std::vector<libusb_transfer*> _transfers;
        libusb_transfer* _probeTransfer;

        std::vector<lgx2::DeviceType> _availableDevices;

        std::function<void(uint8_t*)> *_onFrameDataCallback;

        uint8_t *_frameBuffer;
        bool _shuttingDown;
    };
}


#endif //LGX2USERSPACE_USBSTREAM_H
