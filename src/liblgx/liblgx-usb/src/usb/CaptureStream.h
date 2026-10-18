#ifndef LGX2USERSPACE_CAPTURESTREAM_H
#define LGX2USERSPACE_CAPTURESTREAM_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <libusb-1.0/libusb.h>

namespace libusb {
    // The video/audio stream on bulk IN 0x83. A pool of asynchronous transfers is kept in flight
    // (completions run on the UsbContext's event thread), and completed chunks go into a bounded
    // queue that the consumer drains with deliver(), so reads never wait on the consumer (see
    // PROTOCOL.md, "Stall & desync").
    class CaptureStream {
    public:
        using ChunkCallback = std::function<void(uint8_t *data, uint32_t length, bool gapped)>;

        // Size of each bulk transfer.
        static constexpr int CHUNK_BYTES = 0x1FC000;

        explicit CaptureStream(libusb_device_handle *dev);
        ~CaptureStream();

        // alignToFrameStart: wait for a transfer that begins with a SAV marker before queueing
        // the rest (true after a fresh bootstrap). When false (a reused configuration, mid-stream)
        // the first chunk is flagged gapped so the parser re-syncs instead.
        void start(bool alignToFrameStart);

        // Hand every queued chunk to the callback.
        void deliver(const ChunkCallback &callback);

        // Drop every queued chunk without copying it out; the next delivered chunk is flagged
        // gapped.
        void discard();

        // Cancel outstanding transfers and wait until every one has completed.
        void stop();

        // The stream has stopped for good (device disconnected, or no frame start found).
        bool failed() const;
        std::string failure() const;

        // Diagnostics: chunks dropped because the bounded queue was full (consumer too slow),
        // and transient transfer errors that were recovered from by marking a gap.
        uint64_t droppedChunks() const;
        uint64_t transferErrors() const;

    private:
        // A pool of outstanding bulk IN URBs keeps the pipeline full regardless of
        // consumer load: with all slots in flight the device NAKs (bulk back-pressure)
        // instead of overflowing, so a stalled consumer loses nothing.
        static constexpr int TRANSFER_POOL_SIZE = 4;

        // Bounded queue between the read thread and the consumer. When full, the oldest
        // chunk is dropped (live video wants the latest data) and the stream is flagged
        // gapped so the parser re-syncs.
        static constexpr int RING_SLOTS = 8;

        static constexpr int MAX_PROBE_ATTEMPTS = 8;

        // Optional raw-chunk capture for offline analysis. Set LGX2_DUMP_FILE=/path/to.cab to
        // record each chunk as [uint32 length][uint8 gapped][length bytes], up to DUMP_MAX_CHUNKS.
        static constexpr uint64_t DUMP_MAX_CHUNKS = 128;

        struct RingSlot {
            uint8_t *buffer;
            uint32_t length{0};
            bool gapped{false};
        };

        // How long stop() waits for cancelled transfers to complete before giving up on them.
        static constexpr auto STOP_TIMEOUT = std::chrono::seconds(2);

        libusb_device_handle *_dev;

        // Transfers in flight. Guarded by _transfersMutex together with _shuttingDown, so a
        // completion cannot resubmit a transfer after stop() has cancelled the rest.
        std::vector<libusb_transfer *> _transfers;
        std::mutex _transfersMutex;
        std::condition_variable _transfersDrained;

        // Transfer pool regions (TRANSFER_POOL_SIZE) followed by ring slot regions (RING_SLOTS).
        uint8_t *_frameBuffer;
        RingSlot _ring[RING_SLOTS];
        int _ringWrite{0};
        int _ringCount{0};
        bool _gapPending{false};

        std::atomic<bool> _shuttingDown{false};
        std::atomic<bool> _hasError{false};
        mutable std::mutex _errorMutex;
        std::string _errorMessage;

        // Cumulative diagnostics (updated by the read thread, read by the consumer).
        std::atomic<uint64_t> _droppedChunks{0};
        std::atomic<uint64_t> _transferErrors{0};

        FILE *_dumpFile{nullptr};
        uint64_t _dumpCount{0};

        std::mutex _queueMutex;
        std::vector<uint8_t> _consumeBuffer;

        int _probeAttempts{0};

        void dumpChunk(const uint8_t *data, uint32_t length, bool gapped);

        // Resubmit a completed transfer, or free it once shutting down.
        void submitTransfer(libusb_transfer *transfer);
        void discardTransfer(libusb_transfer *transfer);
        void signalError(const char *message);
        void markGap();
        bool recordProbeAttempt();

        void enqueueChunk(const uint8_t *data, uint32_t length, bool gapped);
        void queueAllFrameReads();
        void addTransferLocked(libusb_transfer *transfer);
        void removeTransferLocked(libusb_transfer *transfer);

        static void probeTransferComplete(libusb_transfer *transfer);
        static void usbTransferComplete(libusb_transfer *transfer);
    };
}

#endif //LGX2USERSPACE_CAPTURESTREAM_H
