#include "CaptureDaemon.h"

#include <cstdio>
#include <exception>
#include <thread>

namespace lgx2d {

    namespace {
        lgx2::CapturePipeline::Options pipelineOptions() {
            lgx2::CapturePipeline::Options options;
            options.keepAliveFrames = true;
            return options;
        }
    }

    CaptureDaemon::CaptureDaemon(lgx2::VideoOutput *video, lgx2::AudioOutput *audio, Options options)
            : _options{options}, _pipeline{video, audio, pipelineOptions()} {
    }

    CaptureDaemon::~CaptureDaemon() {
        endSession("shutting down");
        std::lock_guard<std::mutex> lock(_eventsMutex);
        for (auto &event : _events) libusb_unref_device(event.device);
        _events.clear();
    }

    void CaptureDaemon::queueEvent(libusb_device *device, bool arrived) {
        // Runs on the libusb event thread: only queue, the main loop does the work.
        libusb_ref_device(device);
        std::lock_guard<std::mutex> lock(_eventsMutex);
        _events.push_back({device, arrived});
    }

    void CaptureDaemon::run(const std::atomic<bool> &stop) {
        printf("Waiting for an LGX2 to be connected\n");
        _context.watchDevices([this](libusb_device *device, bool arrived) { queueEvent(device, arrived); });

        while (!stop) {
            handleEvents();

            if (_session && _session->ended()) {
                endSession(_session->endReason());
            }
            auto now = lgx2::CapturePipeline::Clock::now();
            _pipeline.setInputState(_session ? _session->inputState() : lgx2::InputState::NoDevice, now);

            if (_session) {
                if (_pipeline.wantsData()) {
                    _session->deliver([this](uint8_t *data, uint32_t length, bool gapped) {
                        _pipeline.feed(data, length, gapped);
                    });
                } else {
                    // No signal: keep the USB reads going but skip copying the data out.
                    _session->discard();
                }
            }
            _pipeline.tick(now);

            std::this_thread::sleep_for(LOOP_INTERVAL);
        }
    }

    void CaptureDaemon::handleEvents() {
        for (;;) {
            HotplugEvent event{};
            {
                std::lock_guard<std::mutex> lock(_eventsMutex);
                if (_events.empty()) return;
                event = _events.front();
                _events.pop_front();
            }

            if (event.arrived) {
                if (_session) {
                    printf("Another LGX2 was connected; only one is captured at a time\n");
                } else {
                    openSession(event.device);
                }
            } else if (event.device == _sessionDevice) {
                endSession("LGX2 disconnected");
            }
            libusb_unref_device(event.device);
        }
    }

    void CaptureDaemon::openSession(libusb_device *device) {
        lgx2::DeviceType type;
        if (!libusb::UsbContext::usable(device, type)) return;

        libusb::DeviceSession::Options options;
        options.statusPoll = _options.statusPoll;

        for (int attempt = 1; attempt <= OPEN_ATTEMPTS; attempt++) {
            try {
                auto session = std::make_unique<libusb::DeviceSession>(device, type, options);
                session->bootstrap();
                session->start();
                _session = std::move(session);
                _sessionDevice = device;
                libusb_ref_device(_sessionDevice);
                printf("Capturing\n");
                return;
            } catch (const std::exception &e) {
                fprintf(stderr, "Could not start capture (attempt %d of %d): %s\n", attempt, OPEN_ATTEMPTS, e.what());
                std::this_thread::sleep_for(OPEN_RETRY_DELAY);
            }
        }
        fprintf(stderr, "Giving up on this LGX2 until it is reconnected\n");
    }

    void CaptureDaemon::endSession(const std::string &reason) {
        if (!_session) return;
        printf("Capture stopped: %s\n", reason.c_str());
        _session->stop();
        _session.reset();
        libusb_unref_device(_sessionDevice);
        _sessionDevice = nullptr;
        printf("Waiting for an LGX2 to be connected\n");
    }
}
