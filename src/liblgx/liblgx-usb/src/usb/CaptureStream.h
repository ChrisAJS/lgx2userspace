#ifndef LGX2USERSPACE_CAPTURESTREAM_H
#define LGX2USERSPACE_CAPTURESTREAM_H

#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <libusb-1.0/libusb.h>

namespace libusb {
    // The video/audio stream on bulk IN 0x83. A pool of asynchronous transfers is kept in flight
    // by a dedicated thread, and completed chunks go into a bounded queue that the consumer drains
    // with deliver(), so reads never wait on the consumer (see PROTOCOL.md, "Stall & desync").
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

        // Hand every queued chunk to the callback. Throws if the stream has failed (device
        // disconnected, or no frame start found).
        void deliver(const ChunkCallback &callback);

        // Cancel outstanding transfers and wait for the read thread to finish.
        void stop();

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

        libusb_device_handle *_dev;

        std::vector<libusb_transfer *> _transfers;

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

        FILE *_dumpFile{nullptr};
        uint64_t _dumpCount{0};

        std::thread _readThread;
        std::mutex _queueMutex;
        std::vector<uint8_t> _consumeBuffer;

        int _probeAttempts{0};

        void readLoop();
        void dumpChunk(const uint8_t *data, uint32_t length, bool gapped);

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

#endif //LGX2USERSPACE_CAPTURESTREAM_H
