#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
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
        void discardTransfer(libusb_transfer *transfer);
        void signalError(const char *message);
        bool recordProbeAttempt();

        void queueAllFrameReads();

    private:
        static constexpr int MAX_QUEUE_DEPTH = 4;

        libusb_device_handle *_dev;

        std::vector<libusb_transfer *> _transfers;
        libusb_transfer *_probeTransfer;

        std::vector<lgx2::DeviceType> _availableDevices;

        std::function<void(uint8_t *)> *_onFrameDataCallback;

        uint8_t *_frameBuffer;
        std::atomic<bool> _shuttingDown{false};
        std::atomic<bool> _hasError{false};
        std::string _errorMessage;

        std::thread _readThread;
        std::mutex _queueMutex;
        std::queue<std::vector<uint8_t>> _frameQueue;
        std::chrono::steady_clock::time_point _lastSubmitTime{};

        static constexpr int MAX_PROBE_ATTEMPTS = 8;
        int _probeAttempts{0};

        void readLoop();
    };
}

#endif //LGX2USERSPACE_USBSTREAM_H
