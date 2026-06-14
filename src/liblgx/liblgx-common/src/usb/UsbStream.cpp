#include "UsbStream.h"

#include <libusb-1.0/libusb.h>
#include <exception>
#include <system_error>
#include <sstream>
#include <algorithm>
#include <cstring>

#include "bootstrap/commanddata_lgx2.h"
#ifdef GC550_SUPPORT
#   include "bootstrap/commanddata_lgx.h"
#endif

static void probeTransferComplete(struct libusb_transfer *transfer) {
    static uint8_t frameStart[] = { 0x00, 0xFF, 0xFF, 0xC0 };
    auto *stream = static_cast<libusb::UsbStream *>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED &&
        memcmp(transfer->buffer, frameStart, 4) == 0) {
#ifdef LGX2_VERBOSE_STATS
        printf("Found initial frame!\n");
#endif
        // Enqueue probe data as the first chunk — it starts at the frame boundary,
        // so discarding it would cause the first produced frame to be a splice of two frames.
        stream->onFrameData(transfer);
        stream->discardTransfer(transfer);
        stream->queueAllFrameReads();
        return;
    }

    // After a correct bootstrap the device should start at a frame boundary immediately.
    // Retrying a handful of times handles any transient buffering but if we never find
    // the marker something is genuinely wrong — exit rather than accumulate garbage.
    if (!stream->recordProbeAttempt()) {
        fprintf(stderr, "Device did not start sending at a frame boundary after multiple attempts — try replugging\n");
        stream->signalError("No frame start found");
        return;
    }

    stream->submitTransfer(transfer);
}

static void usbTransferComplete(struct libusb_transfer *transfer) {
    auto *stream = static_cast<libusb::UsbStream *>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        stream->onFrameData(transfer);
    } else if (transfer->status != LIBUSB_TRANSFER_CANCELLED) {
        const char *msg = transfer->status == LIBUSB_TRANSFER_NO_DEVICE
            ? "USB capture device was disconnected"
            : "USB read error during capture";
        fprintf(stderr, "%s (status %d)\n", msg, transfer->status);
        stream->signalError(msg);
    }

    stream->submitTransfer(transfer);
}


namespace libusb {
    static const int LGX_DATA_FRAME_LEN = 0x1FC000;

    UsbStream::UsbStream() : _dev{nullptr}, _onFrameDataCallback{} {
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
        _frameBuffer = new uint8_t[LGX_DATA_FRAME_LEN * 16];
    }

    UsbStream::~UsbStream() {
        libusb_exit(nullptr);
    }

    bool UsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
        return std::find(_availableDevices.begin(), _availableDevices.end(), deviceType) != _availableDevices.end();
    }

    void UsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
        std::string targetCommands;
        if (deviceType == lgx2::DeviceType::LGX2) {
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x0551);
            targetCommands = lgx2_setup_commands;
        } else {
#ifdef GC550_SUPPORT
            _dev = libusb_open_device_with_vid_pid(nullptr, 0x07ca, 0x4710);
            targetCommands = lgx_setup_commands;
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
                    "Could not claim interface for the device - is something else using the device?\n");
        }

        int check = 0;
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x03, _frameBuffer, LGX_DATA_FRAME_LEN, &check, 100);
        if (res != LIBUSB_ERROR_TIMEOUT) {
            throw std::runtime_error("Device already been bootstrapped. This results in garbage output. Please unplug and re-plug in the device.");
        } else {
            printf("Bootstrapping the device\n");
        }

        int actualLength;
        uint8_t transferBuffer[512]{0};

        auto commands = std::istringstream{targetCommands};
        std::string command;
        while (commands >> command) {
            if (command[0] == '>') {
                int commandLength = (int) (command.length() - 1) / 2;
                for (int i = 0; i < commandLength; i++) {
                    transferBuffer[i] = std::stoi(command.substr(1 + i * 2, 2), nullptr, 16);
                }
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, transferBuffer, static_cast<int>(commandLength),
                                     &actualLength, 0);
                if (actualLength != commandLength) {
                    throw std::runtime_error("Under-run when attempting to write command data for setup");
                }
            } else {
                int bytesToRead = std::stoi(command.substr(1));
                libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, transferBuffer, bytesToRead, &actualLength, 0);
                if (actualLength != bytesToRead) {
                    throw std::runtime_error("Under-run when attempting to read data during setup");
                }
            }
        }

        printf("Bootstrapping complete\n");
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *)> *onData) {
        _onFrameDataCallback = onData;

        _probeTransfer = libusb_alloc_transfer(0);

        libusb_fill_bulk_transfer(_probeTransfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                  _frameBuffer, LGX_DATA_FRAME_LEN,
                                  probeTransferComplete, this, 0);

        _transfers.push_back(_probeTransfer);
        libusb_submit_transfer(_probeTransfer);

        _readThread = std::thread(&UsbStream::readLoop, this);
    }

    void UsbStream::update() {
        if (_hasError.load(std::memory_order_acquire)) {
            throw std::runtime_error(_errorMessage);
        }

        std::vector<uint8_t> frame;
        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            if (_frameQueue.empty()) return;
            frame = std::move(_frameQueue.front());
            _frameQueue.pop();
        }
        (*_onFrameDataCallback)(frame.data());
    }

    void UsbStream::onFrameData(libusb_transfer *transfer) {
        if (_shuttingDown) return;
        std::lock_guard<std::mutex> lock(_queueMutex);
        if (static_cast<int>(_frameQueue.size()) < MAX_QUEUE_DEPTH) {
            _frameQueue.emplace(transfer->buffer, transfer->buffer + transfer->actual_length);
        }
    }

    void UsbStream::readLoop() {
        struct timeval tv{0, 100000};  // 100ms timeout so shutdown is responsive
        bool cancelledAll = false;

        while (true) {
            libusb_handle_events_timeout(nullptr, &tv);

            if (_shuttingDown && !cancelledAll) {
                cancelledAll = true;
                for (auto *transfer : _transfers) {
                    libusb_cancel_transfer(transfer);
                }
            }

            if (_transfers.empty()) break;
        }
    }

    void UsbStream::shutdownStream() {
        _shuttingDown = true;
        _readThread.join();

        delete[] _frameBuffer;
        libusb_release_interface(_dev, 0);
        libusb_reset_device(_dev);
        libusb_close(_dev);
    }

    void UsbStream::submitTransfer(libusb_transfer *transfer) {
        if (!_shuttingDown) {
            libusb_submit_transfer(transfer);
        } else {
            libusb_free_transfer(transfer);
            _transfers.erase(std::find(_transfers.begin(), _transfers.end(), transfer));
        }
    }

    void UsbStream::discardTransfer(libusb_transfer *transfer) {
        _transfers.erase(std::find(_transfers.begin(), _transfers.end(), transfer));
        libusb_free_transfer(transfer);
    }

    bool UsbStream::recordProbeAttempt() {
        return ++_probeAttempts <= MAX_PROBE_ATTEMPTS;
    }

    void UsbStream::signalError(const char *message) {
        _errorMessage = message;
        _hasError.store(true, std::memory_order_release);
    }

    void UsbStream::queueAllFrameReads() {
        libusb_transfer *transfer = libusb_alloc_transfer(0);

        libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                  _frameBuffer, LGX_DATA_FRAME_LEN,
                                  usbTransferComplete, this, 0);

        _transfers.push_back(transfer);
        libusb_submit_transfer(transfer);
    }
}