#ifndef LGX2USERSPACE_USBCONTEXT_H
#define LGX2USERSPACE_USBCONTEXT_H

#include <atomic>
#include <functional>
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

        // Whether `device` is a supported card on a USB 3 link; reports why not otherwise.
        static bool usable(libusb_device *device, lgx2::DeviceType &type);

        // Called for each LGX2 that arrives or leaves; cards already attached are reported as
        // arrivals straight away. Runs on the event thread (or the calling thread, for the initial
        // arrivals), so it must only queue work: opening a device from it is not allowed.
        using HotplugCallback = std::function<void(libusb_device *device, bool arrived)>;
        void watchDevices(HotplugCallback callback);

    private:
        libusb_context *_context{nullptr};
        std::thread _eventThread;
        std::atomic<bool> _stopping{false};

        HotplugCallback _hotplugCallback;
        libusb_hotplug_callback_handle _hotplugHandle{0};
        bool _watching{false};

        void eventLoop();
        static int hotplugEvent(libusb_context *context, libusb_device *device, libusb_hotplug_event event,
                                void *userData);
    };
}

#endif //LGX2USERSPACE_USBCONTEXT_H
