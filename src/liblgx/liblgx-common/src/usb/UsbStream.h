#ifndef LGX2USERSPACE_USBSTREAM_H
#define LGX2USERSPACE_USBSTREAM_H

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <libusb-1.0/libusb.h>
#include "lgxdevice.h"
#include "SignalStatus.h"

namespace libusb {

    class UsbStream : public lgx2::Stream {
    public:
        explicit UsbStream();
        ~UsbStream() override;

        bool deviceAvailable(lgx2::DeviceType deviceType) override;
        void streamSetupCommands(lgx2::DeviceType deviceType) override;
        void queueFrameRead(std::function<void(uint8_t *, uint32_t, bool)> *onData) override;
        void update() override;

        void shutdownStream() override;

        // Diagnostics: chunks dropped because the bounded queue was full (consumer too slow),
        // and transient transfer errors that were recovered from by marking a gap.
        uint64_t droppedChunks() const override;
        uint64_t transferErrors() const override;

        // From the HDMI status poll. True when the poll is disabled or has given up, so a broken
        // poll never mutes audio permanently.
        bool signalPresent() const override;

    private:
        // A pool of outstanding bulk IN URBs keeps the pipeline full regardless of
        // consumer load: with all slots in flight the device NAKs (bulk back-pressure)
        // instead of overflowing, so a stalled consumer loses nothing.
        static constexpr int TRANSFER_POOL_SIZE = 4;

        // Bounded queue between the read thread and the consumer. When full, the oldest
        // chunk is dropped (live video wants the latest data) and the stream is flagged
        // gapped so the parser re-syncs.
        static constexpr int RING_SLOTS = 8;

        struct RingSlot {
            uint8_t *buffer;
            uint32_t length{0};
            bool gapped{false};
        };

        libusb_device_handle *_dev;

        std::vector<libusb_transfer *> _transfers;
        libusb_transfer *_probeTransfer;

        std::vector<lgx2::DeviceType> _availableDevices;

        std::function<void(uint8_t *, uint32_t, bool)> *_onFrameDataCallback;

        // Transfer pool regions (TRANSFER_POOL_SIZE) followed by ring slot regions (RING_SLOTS).
        uint8_t *_frameBuffer;
        RingSlot _ring[RING_SLOTS];
        int _ringWrite{0};
        int _ringCount{0};
        bool _gapPending{false};

        std::atomic<bool> _shuttingDown{false};
        std::atomic<bool> _hasError{false};
        std::string _errorMessage;

        // Cumulative diagnostics (updated by the read thread, read by the consumer).
        std::atomic<uint64_t> _droppedChunks{0};
        std::atomic<uint64_t> _transferErrors{0};

        // Optional raw-chunk capture for offline analysis. Set LGX2_DUMP_FILE=/path/to.cab to
        // record each chunk as [uint32 length][uint8 gapped][length bytes], up to DUMP_MAX_CHUNKS.
        static constexpr uint64_t DUMP_MAX_CHUNKS = 128;
        FILE *_dumpFile{nullptr};
        uint64_t _dumpCount{0};

        void dumpChunk(const uint8_t *data, uint32_t length, bool gapped);

        std::thread _readThread;
        std::mutex _queueMutex;
        std::vector<uint8_t> _consumeBuffer;

        static constexpr int MAX_PROBE_ATTEMPTS = 8;
        int _probeAttempts{0};

        // Whether the board still holds a configuration from an earlier bootstrap (which
        // survives a USB reset), read back from registers the bootstrap sets.
        enum class BoardState { Fresh, Configured, Unknown };
        bool _alreadyBootstrapped{false};

        BoardState detectLgx2BoardState();
        void runSetupCommands(const std::string &targetCommands);

        // Periodic read-only HDMI input status poll (GC551 only), modelled on the Windows
        // driver's poll threads. It issues I2C reads over the command endpoints (bulk OUT/IN
        // 0x01) while the stream runs on IN 0x83, logs changes in cable/signal/resolution, and
        // drives audio muting on signal loss. Set LGX2_NO_STATUS_POLL=1 to disable it.
        // 50 ms matches the Windows driver's fastest poll (its front-end thread) and bounds how
        // much garbage audio can play after a disconnect; one poll takes ~6 ms.
        static constexpr auto STATUS_POLL_INTERVAL = std::chrono::milliseconds(50);
        // Matches the Windows driver, whose synchronous URB helper (FUN_1400157c4) waits up to
        // 5 s per command transfer: replies can be slow while video is streaming. Note that a
        // poll in flight at shutdown can delay exit by up to this long per transfer.
        static constexpr unsigned int COMMAND_TIMEOUT_MS = 5000;
        // wMaxPacketSize of the command endpoints (SuperSpeed bulk).
        static constexpr int COMMAND_MAX_PACKET = 1024;
        static constexpr int STATUS_MAX_CONSECUTIVE_FAILURES = 10;

        lgx2::DeviceType _deviceType{lgx2::DeviceType::LGX2};
        std::atomic<bool> _signalPresent{true};
        std::thread _statusThread;
        std::mutex _statusMutex;
        std::condition_variable _statusWake;

        // Which half of the last failed I2C read failed: "request (OUT 0x01)" or "reply (IN 0x81)".
        const char *_lastCommandFailure{""};

        void statusPollLoop();
        void probeStatusBeforeStreaming();
        int readAdvStatus(lgx2::AdvStatusRegisters &regs);
        int i2cRead(uint8_t address, uint8_t reg, uint8_t *out, uint8_t count);
        int i2cWrite(uint8_t address, uint8_t reg, uint8_t value);
        void setAutoInputColourSpace();
        void drainCommandResponses();

        void readLoop();

        void submitTransfer(libusb_transfer *transfer);
        void discardTransfer(libusb_transfer *transfer);
        void signalError(const char *message);
        void markGap();
        bool recordProbeAttempt();

        void enqueueChunk(const uint8_t *data, uint32_t length, bool gapped);
        void queueAllFrameReads();

        static void probeTransferComplete(libusb_transfer *transfer);
        static void usbTransferComplete(libusb_transfer *transfer);
    };
}

#endif //LGX2USERSPACE_USBSTREAM_H
