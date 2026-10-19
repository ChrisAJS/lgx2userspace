#ifndef LGX2USERSPACE_CAPTUREDAEMON_H
#define LGX2USERSPACE_CAPTUREDAEMON_H

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <usb/UsbContext.h>
#include <usb/DeviceSession.h>
#include <CapturePipeline.h>

namespace lgx2d {
    // Waits for a card (an LGX2 by default, or an LGX) to be plugged in, bootstraps it and captures
    // from it until it is unplugged, then waits again. The capture goes through a CapturePipeline with keep-alive frames, so the
    // video output always has frames: the picture, or a card saying why there is none.
    class CaptureDaemon {
    public:
        struct Options {
            bool statusPoll{true};
            // The kind of card to wait for; only one kind is captured per run.
            lgx2::DeviceType deviceType{lgx2::DeviceType::LGX2};
        };

        CaptureDaemon(lgx2::VideoOutput *video, lgx2::AudioOutput *audio, Options options);
        ~CaptureDaemon();

        // Runs until `stop` becomes true.
        void run(const std::atomic<bool> &stop);

    private:
        // How often the loop runs: frequent enough to keep up with ~120 USB chunks per second
        // (the capture queue holds about 65 ms) and to pace keep-alive frames.
        static constexpr auto LOOP_INTERVAL = std::chrono::milliseconds(2);
        // A card can take a moment after arrival before it can be opened.
        static constexpr int OPEN_ATTEMPTS = 5;
        static constexpr auto OPEN_RETRY_DELAY = std::chrono::milliseconds(300);

        struct HotplugEvent {
            libusb_device *device; // referenced while queued
            bool arrived;
        };

        // Declared first so it is destroyed last: sessions need its event thread.
        libusb::UsbContext _context;
        Options _options;
        lgx2::CapturePipeline _pipeline;

        std::mutex _eventsMutex;
        std::deque<HotplugEvent> _events;

        std::unique_ptr<libusb::DeviceSession> _session;
        libusb_device *_sessionDevice{nullptr}; // referenced while the session exists

        // "LGX2" or "LGX", for messages.
        const char *cardName() const;

        void queueEvent(libusb_device *device, bool arrived);
        void handleEvents();
        void openSession(libusb_device *device);
        void endSession(const std::string &reason);
    };
}

#endif //LGX2USERSPACE_CAPTUREDAEMON_H
