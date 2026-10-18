#include "DeviceSession.h"

#include <cstdio>
#include <stdexcept>

#include "Bootstrapper.h"

namespace libusb {

    DeviceSession::DeviceSession(libusb_device *device, lgx2::DeviceType type, Options options)
            : _type{type}, _options{options} {
        if (libusb_open(device, &_dev) != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                    "Failed to open device - ensure you have installed the udev rules (and restarted udev if necessary!)");
        }

        if (libusb_set_configuration(_dev, 1) != LIBUSB_SUCCESS) {
            libusb_close(_dev);
            throw std::runtime_error("Failed to set configuration\n");
        }

        if (libusb_claim_interface(_dev, 0) != LIBUSB_SUCCESS) {
            libusb_close(_dev);
            throw std::runtime_error(
                    "Could not claim interface for the device - is something else using the device?");
        }

        _bridge = std::make_unique<CommandBridge>(_dev);
        _statusMonitor = std::make_unique<StatusMonitor>(*_bridge);
    }

    DeviceSession::~DeviceSession() {
        stop();
    }

    bool DeviceSession::statusPollEnabled() const {
        return _type == lgx2::DeviceType::LGX2 && _options.statusPoll;
    }

    void DeviceSession::bootstrap() {
        _alreadyBootstrapped = Bootstrapper{_dev, *_bridge}.bootstrap(_type);
        if (statusPollEnabled()) {
            _statusMonitor->probeOnce();
        }
    }

    void DeviceSession::start() {
        _capture = std::make_unique<CaptureStream>(_dev);
        _capture->start(!_alreadyBootstrapped);

        if (statusPollEnabled()) {
            _statusMonitor->start();
        } else if (_type == lgx2::DeviceType::LGX2) {
            printf("HDMI input status poll disabled\n");
        }
    }

    void DeviceSession::deliver(const CaptureStream::ChunkCallback &callback) {
        if (_capture) _capture->deliver(callback);
    }

    void DeviceSession::discard() {
        if (_capture) _capture->discard();
    }

    void DeviceSession::stop() {
        if (_dev == nullptr) return;

        // Stop everything that uses the device before releasing it.
        if (_statusMonitor) _statusMonitor->stop();
        if (_capture) _capture->stop();
        _capture.reset();
        _statusMonitor.reset();
        _bridge.reset();

        libusb_release_interface(_dev, 0);
        libusb_reset_device(_dev);
        libusb_close(_dev);
        _dev = nullptr;
    }

    bool DeviceSession::ended() const {
        return _dev == nullptr || (_capture && _capture->failed());
    }

    std::string DeviceSession::endReason() const {
        if (_capture && _capture->failed()) return _capture->failure();
        return _dev == nullptr ? "Session stopped" : "";
    }

    lgx2::InputState DeviceSession::inputState() const {
        return _statusMonitor ? _statusMonitor->inputState() : lgx2::InputState::Signal;
    }

    uint64_t DeviceSession::droppedChunks() const {
        return _capture ? _capture->droppedChunks() : 0;
    }

    uint64_t DeviceSession::transferErrors() const {
        return _capture ? _capture->transferErrors() : 0;
    }
}
