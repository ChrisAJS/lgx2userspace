#include "UsbStream.h"

#include <libusb-1.0/libusb.h>
#include <cstdlib>
#include <exception>
#include <system_error>
#include <sstream>
#include <algorithm>
#include <cstring>

#include "bootstrap/commanddata_lgx2.h"
#ifdef GC550_SUPPORT
#   include "bootstrap/commanddata_lgx.h"
#endif

namespace libusb {
    static const int LGX_DATA_FRAME_LEN = 0x1FC000;

    void UsbStream::probeTransferComplete(libusb_transfer *transfer) {
        static uint8_t frameStart[] = { 0x00, 0xFF, 0xFF, 0xC0 };
        auto *stream = static_cast<UsbStream *>(transfer->user_data);

        if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
            // Shutdown in progress; submitTransfer below frees the transfer. Do not count a
            // cancellation as a probe attempt.
            stream->submitTransfer(transfer);
            return;
        }

        if (transfer->status == LIBUSB_TRANSFER_COMPLETED &&
            transfer->actual_length >= static_cast<int>(sizeof(frameStart)) &&
            memcmp(transfer->buffer, frameStart, sizeof(frameStart)) == 0) {
#ifdef LGX2_VERBOSE_STATS
            printf("Found initial frame!\n");
#endif
            // Enqueue probe data as the first chunk — it starts at the frame boundary,
            // so discarding it would cause the first produced frame to be a splice of two frames.
            stream->enqueueChunk(transfer->buffer, transfer->actual_length, false);
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
            // Free and remove the probe transfer so it is not orphaned (which would prevent
            // the read loop from ever draining _transfers and hanging shutdown).
            stream->discardTransfer(transfer);
            return;
        }

        stream->submitTransfer(transfer);
    }

    void UsbStream::usbTransferComplete(libusb_transfer *transfer) {
        auto *stream = static_cast<UsbStream *>(transfer->user_data);

        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
                // A short read means bytes were lost mid-stream: flag the gap so the parser re-syncs.
                stream->enqueueChunk(transfer->buffer, transfer->actual_length,
                                     transfer->actual_length < LGX_DATA_FRAME_LEN);
                break;
            case LIBUSB_TRANSFER_CANCELLED:
                // Shutdown in progress; submitTransfer below frees the transfer.
                break;
            case LIBUSB_TRANSFER_NO_DEVICE:
                fprintf(stderr, "USB capture device was disconnected\n");
                stream->signalError("USB capture device was disconnected");
                break;
            default:
                // Transient error (bus reset, etc.): keep the pipeline alive and mark a gap.
                fprintf(stderr, "USB read error during capture (status %d) — continuing\n", transfer->status);
                stream->markGap();
                stream->_transferErrors.fetch_add(1, std::memory_order_relaxed);
        }

        stream->submitTransfer(transfer);
    }

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
        _frameBuffer = new uint8_t[LGX_DATA_FRAME_LEN * (TRANSFER_POOL_SIZE + RING_SLOTS)];
        for (int i = 0; i < RING_SLOTS; i++) {
            _ring[i].buffer = _frameBuffer + (TRANSFER_POOL_SIZE + i) * LGX_DATA_FRAME_LEN;
        }

        if (const char *dumpPath = std::getenv("LGX2_DUMP_FILE")) {
            _dumpFile = fopen(dumpPath, "wb");
            if (_dumpFile != nullptr) {
                printf("Dumping up to %llu raw chunks to %s\n",
                       (unsigned long long) DUMP_MAX_CHUNKS, dumpPath);
            } else {
                fprintf(stderr, "Could not open LGX2_DUMP_FILE '%s' for writing\n", dumpPath);
            }
        }
    }

    UsbStream::~UsbStream() {
        libusb_exit(nullptr);
    }

    bool UsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
        return std::find(_availableDevices.begin(), _availableDevices.end(), deviceType) != _availableDevices.end();
    }

    void UsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
        _deviceType = deviceType;
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
                    "Could not claim interface for the device - is something else using the device?");
        }

        BoardState boardState = deviceType == lgx2::DeviceType::LGX2 ? detectLgx2BoardState() : BoardState::Unknown;
        if (boardState == BoardState::Configured) {
            // Re-running the bootstrap on a configured board is what produced garbage output, and
            // the configuration survives a USB reset, so keep it and just start streaming.
            printf("Device is already bootstrapped - reusing its existing configuration\n");
            _alreadyBootstrapped = true;
        } else {
            if (boardState == BoardState::Unknown) {
                // Board state could not be read: fall back to checking whether video is flowing.
                int check = 0;
                int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x03, _frameBuffer, LGX_DATA_FRAME_LEN,
                                               &check, 100);
                if (res != LIBUSB_ERROR_TIMEOUT) {
                    throw std::runtime_error("Device already been bootstrapped. This results in garbage output. Please unplug and re-plug in the device.");
                }
            }
            printf("Bootstrapping the device\n");
            runSetupCommands(targetCommands);
            printf("Bootstrapping complete\n");

            // The LGX2 command capture was cut off mid-transaction: it ends with a read request
            // (>02220102A8) whose 2-byte reply is never read. Collect it now, otherwise it sits in
            // IN 0x81 and is returned as the answer to the next command read.
            drainCommandResponses();
        }

        if (deviceType == lgx2::DeviceType::LGX2) {
            setAutoInputColourSpace();
            if (std::getenv("LGX2_NO_STATUS_POLL") == nullptr) {
                probeStatusBeforeStreaming();
            }
        }
    }

    void UsbStream::setAutoInputColourSpace() {
        // The replayed bootstrap leaves the ADV7604's input colour space (IO 0x02 bits 7:4) fixed to
        // full-range RGB (0x1C), as the Windows driver set it for the source connected when the
        // capture was taken. A YCbCr source (e.g. a camera) is then converted as if it were RGB and
        // its colour is destroyed. Use automatic mode instead (0xF: chosen from the source's AVI
        // InfoFrame), as Linux adv7604.c does for HDMI, keeping the output-format bits.
        constexpr uint8_t ADV_IO = 0x20;
        uint8_t io02 = 0;
        int res = i2cRead(ADV_IO, 0x02, &io02, 1);
        if (res != LIBUSB_SUCCESS) {
            fprintf(stderr, "Could not read the receiver's input colour space (%s on %s)\n",
                    libusb_error_name(res), _lastCommandFailure);
            drainCommandResponses();
            return;
        }
        uint8_t automatic = static_cast<uint8_t>((io02 & 0x0F) | 0xF0);
        if (automatic == io02) return;
        if (i2cWrite(ADV_IO, 0x02, automatic) != LIBUSB_SUCCESS) {
            fprintf(stderr, "Could not set the receiver's input colour space to automatic\n");
            return;
        }
        printf("Receiver input colour space: automatic (IO 0x02 0x%02X -> 0x%02X)\n", io02, automatic);
    }

    int UsbStream::i2cWrite(uint8_t address, uint8_t reg, uint8_t value) {
        // Bridge write request: 01 <addr> <reg addr length=1> <reg> <data>; no reply.
        uint8_t request[] = {0x01, address, 0x01, reg, value};
        int actualLength = 0;
        _lastCommandFailure = "request (OUT 0x01)";
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, request, sizeof(request),
                                       &actualLength, COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        return actualLength == static_cast<int>(sizeof(request)) ? LIBUSB_SUCCESS : LIBUSB_ERROR_IO;
    }

    UsbStream::BoardState UsbStream::detectLgx2BoardState() {
        // A reply left over from an interrupted previous session would be read as our answer.
        drainCommandResponses();

        // The PCA9539A GPIO expander powers up with both configuration registers at 0xFF (all
        // inputs); the bootstrap sets them to 00/09. It is bus powered and unaffected by a USB
        // reset, so these values identify a board that is still configured from an earlier run.
        constexpr uint8_t PCA9539A = 0x74;
        uint8_t pcaConfig[2];
        if (i2cRead(PCA9539A, 0x06, pcaConfig, sizeof(pcaConfig)) != LIBUSB_SUCCESS) {
            drainCommandResponses();
            return BoardState::Unknown;
        }
        if (pcaConfig[0] == 0xFF && pcaConfig[1] == 0xFF) {
            return BoardState::Fresh;
        }

        // Confirm with the ADV7604, whose EDID map address (IO 0xFA) the bootstrap sets to 0x6C.
        constexpr uint8_t ADV_IO = 0x20;
        uint8_t edidMapAddress = 0;
        if (pcaConfig[0] == 0x00 && pcaConfig[1] == 0x09 &&
            i2cRead(ADV_IO, 0xFA, &edidMapAddress, 1) == LIBUSB_SUCCESS && edidMapAddress == 0x6C) {
            return BoardState::Configured;
        }

        drainCommandResponses();
        fprintf(stderr, "Unrecognised board state (PCA9539A config %02X %02X, ADV7604 EDID map 0x%02X)\n",
                pcaConfig[0], pcaConfig[1], edidMapAddress);
        return BoardState::Unknown;
    }

    void UsbStream::runSetupCommands(const std::string &targetCommands) {
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
    }

    void UsbStream::queueFrameRead(std::function<void(uint8_t *, uint32_t, bool)> *onData) {
        _onFrameDataCallback = onData;

        if (_alreadyBootstrapped) {
            // The device is mid-stream with whatever it buffered since the last session, so the
            // first transfer will not start on a frame boundary. Skip the alignment probe and let
            // the FrameBuilder re-sync on the first clean SAV->EAV pair instead.
            markGap();
            queueAllFrameReads();
        } else {
            _probeTransfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(_probeTransfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer, LGX_DATA_FRAME_LEN,
                                      probeTransferComplete, this, 0);

            _transfers.push_back(_probeTransfer);
            libusb_submit_transfer(_probeTransfer);
        }

        _readThread = std::thread(&UsbStream::readLoop, this);

        // The register map is ADV7604-specific, which only the GC551 has.
        if (_deviceType == lgx2::DeviceType::LGX2) {
            if (std::getenv("LGX2_NO_STATUS_POLL") == nullptr) {
                _statusThread = std::thread(&UsbStream::statusPollLoop, this);
            } else {
                printf("HDMI input status poll disabled (LGX2_NO_STATUS_POLL)\n");
            }
        }
    }

    void UsbStream::statusPollLoop() {
        lgx2::SignalStatus lastStatus;
        bool haveStatus = false;
        int consecutiveFailures = 0;

        std::unique_lock<std::mutex> lock(_statusMutex);
        while (!_shuttingDown) {
            lock.unlock();

            lgx2::AdvStatusRegisters regs;
            auto pollStart = std::chrono::steady_clock::now();
            int res = readAdvStatus(regs);
            auto pollMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - pollStart).count();
            if (res == LIBUSB_ERROR_NO_DEVICE) {
                // The stream's own transfers report the disconnect.
                return;
            }

            if (res != LIBUSB_SUCCESS) {
                if (_shuttingDown) return;
                // A request may have been answered late: flush it so the next reply is not
                // attributed to the wrong register.
                drainCommandResponses();
                if (++consecutiveFailures == STATUS_MAX_CONSECUTIVE_FAILURES) {
                    fprintf(stderr, "HDMI input status poll failed %d times in a row (%s on %s, last attempt %lld ms) — stopping the poll\n",
                            consecutiveFailures, libusb_error_name(res), _lastCommandFailure,
                            static_cast<long long>(pollMs));
                    return;
                }
            } else {
                consecutiveFailures = 0;
                lgx2::SignalStatus status = lgx2::SignalStatus::decode(regs);
                if (!haveStatus || status != lastStatus) {
                    // The poll duration shows how slowly the bridge answers mid-stream.
                    printf("HDMI input: %s (status poll took %lld ms)\n", status.describe().c_str(),
                           static_cast<long long>(pollMs));
                    lastStatus = status;
                    haveStatus = true;
                }
            }

            lock.lock();
            _statusWake.wait_for(lock, STATUS_POLL_INTERVAL, [this] { return _shuttingDown.load(); });
        }
    }

    int UsbStream::readAdvStatus(lgx2::AdvStatusRegisters &regs) {
        // 7-bit I2C addresses of the ADV7604 maps, as programmed by the bootstrap.
        constexpr uint8_t ADV_IO = 0x20;
        constexpr uint8_t ADV_CP = 0x22;
        constexpr uint8_t ADV_HDMI = 0x34;
        constexpr uint8_t ADV_INFOFRAME = 0x3E;

        int res;
        if ((res = i2cRead(ADV_IO, 0x0C, &regs.io0c, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_IO, 0x12, &regs.io12, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_IO, 0x6A, &regs.io6a, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_IO, 0x6F, &regs.io6f, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_CP, 0xB1, &regs.cpB1, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_IO, 0x02, &regs.io02, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_IO, 0x60, &regs.io60, 1)) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_INFOFRAME, 0x01, &regs.avi01, 1)) != LIBUSB_SUCCESS) return res;
        // Block reads stay at or below 13 bytes, the largest read seen in the bootstrap capture.
        if ((res = i2cRead(ADV_HDMI, 0x05, regs.hdmi05to0c, sizeof(regs.hdmi05to0c))) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_HDMI, 0x20, regs.hdmi20to25, sizeof(regs.hdmi20to25))) != LIBUSB_SUCCESS) return res;
        if ((res = i2cRead(ADV_HDMI, 0x2A, regs.hdmi2ato35, sizeof(regs.hdmi2ato35))) != LIBUSB_SUCCESS) return res;
        return i2cRead(ADV_HDMI, 0x3B, &regs.hdmi3b, 1);
    }

    int UsbStream::i2cRead(uint8_t address, uint8_t reg, uint8_t *out, uint8_t count) {
        // Bridge read request: 02 <addr> <reg addr length=1> <count> <reg>, answered by
        // exactly <count> bytes on bulk IN 0x01 (see PROTOCOL.md, "Bootstrap command stream").
        uint8_t request[] = {0x02, address, 0x01, count, reg};
        int actualLength = 0;

        _lastCommandFailure = "request (OUT 0x01)";
        int res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_OUT | 0x01, request, sizeof(request),
                                       &actualLength, COMMAND_TIMEOUT_MS);
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != static_cast<int>(sizeof(request))) return LIBUSB_ERROR_IO;

        // Receive into a full max-packet buffer: asking for exactly <count> bytes turns a stale or
        // unexpected reply into LIBUSB_ERROR_OVERFLOW, which can leave the endpoint unusable.
        _lastCommandFailure = "reply (IN 0x81)";
        uint8_t reply[COMMAND_MAX_PACKET];
        res = libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, reply, sizeof(reply), &actualLength,
                                   COMMAND_TIMEOUT_MS);
        if (res == LIBUSB_ERROR_PIPE) {
            libusb_clear_halt(_dev, LIBUSB_ENDPOINT_IN | 0x01);
        }
        if (res != LIBUSB_SUCCESS) return res;
        if (actualLength != count) {
            _lastCommandFailure = "reply (IN 0x81, unexpected length)";
            return LIBUSB_ERROR_IO;
        }
        memcpy(out, reply, count);
        return LIBUSB_SUCCESS;
    }

    void UsbStream::probeStatusBeforeStreaming() {
        // Diagnostic: the bootstrap's own reads succeed before the IN 0x83 stream starts, so a
        // status read here tells apart "request format wrong" from "bridge does not answer
        // while streaming" when the periodic poll fails.
        lgx2::AdvStatusRegisters regs;
        auto start = std::chrono::steady_clock::now();
        int res = readAdvStatus(regs);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        if (res == LIBUSB_SUCCESS) {
            printf("HDMI input (before streaming): %s (status read took %lld ms)\n",
                   lgx2::SignalStatus::decode(regs).describe().c_str(), static_cast<long long>(ms));
        } else {
            fprintf(stderr, "HDMI input status read before streaming failed: %s on %s after %lld ms\n",
                    libusb_error_name(res), _lastCommandFailure, static_cast<long long>(ms));
            drainCommandResponses();
        }
    }

    void UsbStream::drainCommandResponses() {
        uint8_t discard[COMMAND_MAX_PACKET];
        int actualLength = 0;
        while (libusb_bulk_transfer(_dev, LIBUSB_ENDPOINT_IN | 0x01, discard, sizeof(discard), &actualLength, 10) ==
               LIBUSB_SUCCESS) {
        }
    }

    void UsbStream::update() {
        if (_hasError.load(std::memory_order_acquire)) {
            throw std::runtime_error(_errorMessage);
        }

        // Drain everything pending so a slow consumer catches up (or drops oldest) in one pass.
        for (;;) {
            uint32_t length = 0;
            bool gapped = false;
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                if (_ringCount == 0) return;

                int readIndex = (_ringWrite - _ringCount + RING_SLOTS) % RING_SLOTS;
                length = _ring[readIndex].length;
                gapped = _ring[readIndex].gapped;
                _consumeBuffer.assign(_ring[readIndex].buffer, _ring[readIndex].buffer + length);
                _ringCount--;
            }
            (*_onFrameDataCallback)(_consumeBuffer.data(), length, gapped);
        }
    }

    void UsbStream::dumpChunk(const uint8_t *data, uint32_t length, bool gapped) {
        if (_dumpFile == nullptr || _dumpCount >= DUMP_MAX_CHUNKS) return;
        struct Header {
            uint32_t length;
            uint8_t gapped;
        } header{length, static_cast<uint8_t>(gapped ? 1 : 0)};
        fwrite(&header, sizeof(header), 1, _dumpFile);
        fwrite(data, 1, length, _dumpFile);
        _dumpCount++;
    }

    void UsbStream::enqueueChunk(const uint8_t *data, uint32_t length, bool gapped) {
        if (_shuttingDown || length == 0) return;
        dumpChunk(data, length, gapped);

        std::lock_guard<std::mutex> lock(_queueMutex);
        if (length > LGX_DATA_FRAME_LEN) length = LGX_DATA_FRAME_LEN;

        if (_ringCount == RING_SLOTS) {
            // Consumer is too slow to keep up: the oldest slot is about to be overwritten,
            // so flag a gap on the chunk that lands in its place.
            _gapPending = true;
            _droppedChunks.fetch_add(1, std::memory_order_relaxed);
        }

        RingSlot &slot = _ring[_ringWrite];
        memcpy(slot.buffer, data, length);
        slot.length = length;
        slot.gapped = _gapPending || gapped;
        _gapPending = false;

        _ringWrite = (_ringWrite + 1) % RING_SLOTS;
        if (_ringCount < RING_SLOTS) _ringCount++;
    }

    void UsbStream::markGap() {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _gapPending = true;
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
        {
            // Taking the lock orders the flag store before the poll thread's predicate check.
            std::lock_guard<std::mutex> lock(_statusMutex);
        }
        _statusWake.notify_all();
        if (_statusThread.joinable()) {
            _statusThread.join();
        }
        _readThread.join();

        if (_dumpFile != nullptr) {
            fclose(_dumpFile);
            _dumpFile = nullptr;
            printf("Wrote %llu raw chunks to dump file\n", (unsigned long long) _dumpCount);
        }

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

    uint64_t UsbStream::droppedChunks() const {
        return _droppedChunks.load(std::memory_order_relaxed);
    }

    uint64_t UsbStream::transferErrors() const {
        return _transferErrors.load(std::memory_order_relaxed);
    }

    void UsbStream::queueAllFrameReads() {
        for (int i = 0; i < TRANSFER_POOL_SIZE; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer + i * LGX_DATA_FRAME_LEN, LGX_DATA_FRAME_LEN,
                                      usbTransferComplete, this, 0);

            _transfers.push_back(transfer);
            libusb_submit_transfer(transfer);
        }
    }
}
