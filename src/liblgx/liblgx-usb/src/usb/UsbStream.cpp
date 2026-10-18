#include "UsbStream.h"

#include <cstdlib>
#include <stdexcept>

namespace libusb {

    UsbStream::UsbStream() : _devices{_context.findDevices()} {
    }

    UsbStream::~UsbStream() {
        shutdownStream();
    }

    bool UsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
        for (const auto &device : _devices) {
            if (device.type() == deviceType) return true;
        }
        return false;
    }

    void UsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
#ifndef GC550_SUPPORT
        if (deviceType != lgx2::DeviceType::LGX2) {
            throw std::runtime_error("Dangerous LGX (GC550) support has not been compiled. Check the README.md");
        }
#endif
        const FoundDevice *target = nullptr;
        for (const auto &device : _devices) {
            if (device.type() == deviceType) {
                target = &device;
                break;
            }
        }
        if (target == nullptr) {
            throw std::runtime_error(
                    "Failed to open device - is it connected? Run lsusb to check and ensure you have installed the udev rules (and restarted udev if necessary!)");
        }

        DeviceSession::Options options;
        options.statusPoll = std::getenv("LGX2_NO_STATUS_POLL") == nullptr;
        _session = std::make_unique<DeviceSession>(target->device(), deviceType, options);
        _session->bootstrap();
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *, uint32_t, bool)> *onData) {
        _onFrameDataCallback = onData;
        _session->start();
    }

    void UsbStream::update() {
        // The viewer has nothing to show without its card, so an ended session stops it.
        if (_session->ended()) {
            throw std::runtime_error(_session->endReason());
        }
        _session->deliver(*_onFrameDataCallback);
    }

    void UsbStream::shutdownStream() {
        if (_session) {
            _session->stop();
            _session.reset();
        }
    }

    uint64_t UsbStream::droppedChunks() const {
        return _session ? _session->droppedChunks() : 0;
    }

    uint64_t UsbStream::transferErrors() const {
        return _session ? _session->transferErrors() : 0;
    }

    lgx2::InputState UsbStream::inputState() const {
        return _session ? _session->inputState() : lgx2::InputState::Signal;
    }
}
