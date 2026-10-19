#include "UsbContext.h"

#include <cstdio>
#include <stdexcept>
#include <string>

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
        if (_watching) libusb_hotplug_deregister_callback(_context, _hotplugHandle);
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

    bool UsbContext::usable(libusb_device *device, lgx2::DeviceType &type) {
        libusb_device_descriptor desc{};
        libusb_get_device_descriptor(device, &desc);
        if (desc.idVendor != 0x07ca) return false;

        if (desc.idProduct == 0x0551) {
            if (desc.bcdUSB >= 0x300) {
                printf("LGX2 (GC551) detected\n");
                type = lgx2::DeviceType::LGX2;
                return true;
            }
            fprintf(stderr, "LGX2 (GC551) detected, but the device is not using USB3. Ensure you are using a USB3 port (often denoted with a specific colour on systems with both USB2 and 3) and that you are using a cable that is capable of USB3 speeds.\n");
        }
#ifdef GC550_SUPPORT
        else if (desc.idProduct == 0x4710) {
            if (desc.bcdUSB >= 0x300) {
                printf("LGX (GC550) detected\n");
                type = lgx2::DeviceType::LGX;
                return true;
            }
            fprintf(stderr, "LGX (GC550) detected, but the device is not using USB3. Ensure you are using a USB3 port (often denoted with a specific colour on systems with both USB2 and 3) and that you are using a cable that is capable of USB3 speeds.\n");
        }
#else
        else if (desc.idProduct == 0x4710) {
            fprintf(stderr, "LGX (GC550) detected - but support for device not compiled in.\n");
        }
#endif
        return false;
    }

    std::vector<FoundDevice> UsbContext::findDevices() {
        std::vector<FoundDevice> found;

        libusb_device **list = nullptr;
        ssize_t count = libusb_get_device_list(_context, &list);

        for (ssize_t idx = 0; idx < count; ++idx) {
            lgx2::DeviceType type;
            if (usable(list[idx], type)) found.emplace_back(list[idx], type);
        }

        libusb_free_device_list(list, 1);
        return found;
    }

    void UsbContext::watchDevices(lgx2::DeviceType type, HotplugCallback callback) {
        if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            throw std::runtime_error("This platform's libusb does not support hotplug notifications");
        }
        _hotplugCallback = std::move(callback);
        const int productId = type == lgx2::DeviceType::LGX2 ? 0x0551 : 0x4710;
        int res = libusb_hotplug_register_callback(
                _context,
                static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
                LIBUSB_HOTPLUG_ENUMERATE, 0x07ca, productId, LIBUSB_HOTPLUG_MATCH_ANY,
                &UsbContext::hotplugEvent, this, &_hotplugHandle);
        if (res != LIBUSB_SUCCESS) {
            throw std::runtime_error(std::string("Could not register for USB hotplug events: ") + libusb_error_name(res));
        }
        _watching = true;
    }

    int UsbContext::hotplugEvent(libusb_context *, libusb_device *device, libusb_hotplug_event event,
                                 void *userData) {
        auto *context = static_cast<UsbContext *>(userData);
        if (context->_hotplugCallback) {
            context->_hotplugCallback(device, event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED);
        }
        return 0; // stay registered
    }
}
