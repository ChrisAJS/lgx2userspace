#ifndef LGX2USERSPACE_USBCONTEXT_H
#define LGX2USERSPACE_USBCONTEXT_H

#include <atomic>
#include <thread>
#include <vector>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"

namespace libusb {
    // An attached capture card found by UsbContext::findDevices(). Holds a reference on the
    // libusb_device until destroyed, so it stays valid even if the card is unplugged meanwhile.
    class FoundDevice {
    public:
        FoundDevice(libusb_device *device, lgx2::DeviceType type);
        ~FoundDevice();
        FoundDevice(FoundDevice &&other) noexcept;
        FoundDevice(const FoundDevice &) = delete;
        FoundDevice &operator=(const FoundDevice &) = delete;
        FoundDevice &operator=(FoundDevice &&) = delete;

        libusb_device *device() const { return _device; }
        lgx2::DeviceType type() const { return _type; }

    private:
        libusb_device *_device;
        lgx2::DeviceType _type;
    };

    // Owns a libusb context and the single thread that handles its events (asynchronous transfer
    // completions, and later hotplug callbacks). Everything USB-related is created against it.
    class UsbContext {
    public:
        UsbContext();
        ~UsbContext();
        UsbContext(const UsbContext &) = delete;
        UsbContext &operator=(const UsbContext &) = delete;

        libusb_context *get() const { return _context; }

        // Supported cards currently attached and on a USB 3 link (others are reported and skipped).
        std::vector<FoundDevice> findDevices();

    private:
        libusb_context *_context{nullptr};
        std::thread _eventThread;
        std::atomic<bool> _stopping{false};

        void eventLoop();
    };
}

#endif //LGX2USERSPACE_USBCONTEXT_H
