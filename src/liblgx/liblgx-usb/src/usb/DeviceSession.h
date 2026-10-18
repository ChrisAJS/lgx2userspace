#ifndef LGX2USERSPACE_DEVICESESSION_H
#define LGX2USERSPACE_DEVICESESSION_H

#include <memory>
#include <string>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"
#include "CommandBridge.h"
#include "StatusMonitor.h"
#include "CaptureStream.h"

namespace libusb {
    // One opened capture card, from claiming it through bootstrap to streaming. A session ends
    // when its device disconnects or the stream fails; ended() reports that instead of throwing,
    // so a caller can decide whether to stop or wait for the card to come back.
    class DeviceSession {
    public:
        struct Options {
            // Poll HDMI input status (ADV7604 registers, GC551 only).
            bool statusPoll{true};
        };

        // Opens and claims the device; throws if it cannot be used.
        DeviceSession(libusb_device *device, lgx2::DeviceType type, Options options);
        ~DeviceSession();
        DeviceSession(const DeviceSession &) = delete;
        DeviceSession &operator=(const DeviceSession &) = delete;

        // Bring the board to a streaming-ready state (reusing an existing configuration when the
        // board still holds one). Throws if the board cannot be used.
        void bootstrap();

        // Start the IN 0x83 stream and the status poll.
        void start();

        // Hand queued stream chunks to the callback.
        void deliver(const CaptureStream::ChunkCallback &callback);

        // Drop queued stream chunks unread (when nothing wants them); reading continues.
        void discard();

        // Stop streaming and polling, then release and reset the device. Idempotent.
        void stop();

        bool ended() const;
        std::string endReason() const;

        lgx2::DeviceType type() const { return _type; }
        lgx2::InputState inputState() const;
        uint64_t droppedChunks() const;
        uint64_t transferErrors() const;

    private:
        libusb_device_handle *_dev{nullptr};
        lgx2::DeviceType _type;
        Options _options;
        bool _alreadyBootstrapped{false};

        std::unique_ptr<CommandBridge> _bridge;
        std::unique_ptr<StatusMonitor> _statusMonitor;
        std::unique_ptr<CaptureStream> _capture;

        bool statusPollEnabled() const;
    };
}

#endif //LGX2USERSPACE_DEVICESESSION_H
