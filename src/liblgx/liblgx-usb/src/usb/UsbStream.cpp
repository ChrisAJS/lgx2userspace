#include "UsbStream.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace libusb {

    UsbStream::UsbStream() {
        libusb_init(nullptr);

        libusb_device **list = nullptr;
        ssize_t count = libusb_get_device_list(nullptr, &list);

        for (ssize_t idx = 0; idx < count; ++idx) {
            libusb_device *device = list[idx];
            libusb_device_descriptor desc{};

            libusb_get_device_descriptor(device, &desc);
            if (desc.idVendor == 0x07ca) {
                if (desc.idProduct == 0x0551) {
                    if (desc.bcdUSB >= 0x300) {
                        printf("LGX2 (GC551) detected\n");
                        _availableDevices.push_back(lgx2::DeviceType::LGX2);
                    } else {
                        fprintf(stderr, "LGX2 (GC551) detected, but the device is not using USB3. Ensure you are using a USB3 port (often denoted with a specific colour on systems with both USB2 and 3) and that you are using a cable that is capable of USB3 speeds.\n");
                    }
                }

#ifdef GC550_SUPPORT
                else if (desc.idProduct == 0x4710) {
                    if (desc.bcdUSB >= 0x300) {
                        printf("LGX (GC550) detected\n");
                        _availableDevices.push_back(lgx2::DeviceType::LGX);
                    } else {
                        fprintf(stderr, "LGX (GC550) detected, but the device is not using USB3. Ensure you are using a USB3 port (often denoted with a specific colour on systems with both USB2 and 3) and that you are using a cable that is capable of USB3 speeds.\n");
                    }
                }
#else
                else if (desc.idProduct == 0x4710) {
                    fprintf(stderr, "LGX (GC550) detected - but support for device not compiled in.\n");
                }
#endif
            }
        }

        libusb_free_device_list(list, (int) count);
    }

    UsbStream::~UsbStream() {
        libusb_exit(nullptr);
    }

    bool UsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
        return std::find(_availableDevices.begin(), _availableDevices.end(), deviceType) != _availableDevices.end();
    }

    bool UsbStream::statusPollEnabled() const {
        return _deviceType == lgx2::DeviceType::LGX2 && std::getenv("LGX2_NO_STATUS_POLL") == nullptr;
    }

    void UsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
        _deviceType = deviceType;
        if (deviceType == lgx2::DeviceType::LGX2) {
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x0551);
        } else {
#ifdef GC550_SUPPORT
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x4710);
#else
            throw std::runtime_error("Dangerous LGX (GC550) support has not been compiled. Check the README.md");
#endif
        }

        if (_dev == nullptr) {
            throw std::runtime_error(
                    "Failed to open device - is it connected? Run lsusb to check and ensure you have installed the udev rules (and restarted udev if necessary!)");
        }

        if (libusb_set_configuration(_dev, 1) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Failed to set configuration\n");
        }

        if (libusb_claim_interface(_dev, 0) != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                    "Could not claim interface for the device - is something else using the device?");
        }

        _bridge = std::make_unique<CommandBridge>(_dev);
        _alreadyBootstrapped = Bootstrapper{_dev, *_bridge}.bootstrap(deviceType);

        _statusMonitor = std::make_unique<StatusMonitor>(*_bridge);
        if (statusPollEnabled()) {
            _statusMonitor->probeOnce();
        }
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *, uint32_t, bool)> *onData) {
        _onFrameDataCallback = onData;

        _capture = std::make_unique<CaptureStream>(_dev);
        _capture->start(!_alreadyBootstrapped);

        if (statusPollEnabled()) {
            _statusMonitor->start();
        } else if (_deviceType == lgx2::DeviceType::LGX2) {
            printf("HDMI input status poll disabled (LGX2_NO_STATUS_POLL)\n");
        }
    }

    void UsbStream::update() {
        _capture->deliver(*_onFrameDataCallback);
    }

    void UsbStream::shutdownStream() {
        // Stop everything that uses the device before releasing it.
        if (_statusMonitor) _statusMonitor->stop();
        if (_capture) _capture->stop();
        _capture.reset();
        _statusMonitor.reset();
        _bridge.reset();

        if (_dev != nullptr) {
            libusb_release_interface(_dev, 0);
            libusb_reset_device(_dev);
            libusb_close(_dev);
            _dev = nullptr;
        }
    }

    uint64_t UsbStream::droppedChunks() const {
        return _capture ? _capture->droppedChunks() : 0;
    }

    uint64_t UsbStream::transferErrors() const {
        return _capture ? _capture->transferErrors() : 0;
    }

    lgx2::InputState UsbStream::inputState() const {
        return _statusMonitor ? _statusMonitor->inputState() : lgx2::InputState::Signal;
    }
}
