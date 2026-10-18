#include "UsbContext.h"

#include <cstdio>
#include <stdexcept>

namespace libusb {

    FoundDevice::FoundDevice(libusb_device *device, lgx2::DeviceType type) : _device{device}, _type{type} {
        libusb_ref_device(_device);
    }

    FoundDevice::~FoundDevice() {
        if (_device != nullptr) libusb_unref_device(_device);
    }

    FoundDevice::FoundDevice(FoundDevice &&other) noexcept : _device{other._device}, _type{other._type} {
        other._device = nullptr;
    }

    UsbContext::UsbContext() {
        if (libusb_init(&_context) != LIBUSB_SUCCESS) {
            throw std::runtime_error("Could not initialise libusb");
        }
        _eventThread = std::thread(&UsbContext::eventLoop, this);
    }

    UsbContext::~UsbContext() {
        _stopping = true;
        libusb_interrupt_event_handler(_context);
        if (_eventThread.joinable()) _eventThread.join();
        libusb_exit(_context);
    }

    void UsbContext::eventLoop() {
        // The timeout only bounds how long a stop request can go unnoticed if the interrupt is
        // missed; libusb_interrupt_event_handler normally wakes the loop straight away.
        struct timeval tv{0, 100000};
        while (!_stopping) {
            libusb_handle_events_timeout_completed(_context, &tv, nullptr);
        }
    }

    std::vector<FoundDevice> UsbContext::findDevices() {
        std::vector<FoundDevice> found;

        libusb_device **list = nullptr;
        ssize_t count = libusb_get_device_list(_context, &list);

        for (ssize_t idx = 0; idx < count; ++idx) {
            libusb_device *device = list[idx];
            libusb_device_descriptor desc{};

            libusb_get_device_descriptor(device, &desc);
            if (desc.idVendor != 0x07ca) continue;

            if (desc.idProduct == 0x0551) {
                if (desc.bcdUSB >= 0x300) {
                    printf("LGX2 (GC551) detected\n");
                    found.emplace_back(device, lgx2::DeviceType::LGX2);
                } else {
                    fprintf(stderr, "LGX2 (GC551) detected, but the device is not using USB3. Ensure you are using a USB3 port (often denoted with a specific colour on systems with both USB2 and 3) and that you are using a cable that is capable of USB3 speeds.\n");
                }
            }
#ifdef GC550_SUPPORT
            else if (desc.idProduct == 0x4710) {
                if (desc.bcdUSB >= 0x300) {
                    printf("LGX (GC550) detected\n");
                    found.emplace_back(device, lgx2::DeviceType::LGX);
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

        libusb_free_device_list(list, 1);
        return found;
    }
}
