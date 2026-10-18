#include "CaptureStream.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace libusb {

    void CaptureStream::probeTransferComplete(libusb_transfer *transfer) {
        static uint8_t frameStart[] = { 0x00, 0xFF, 0xFF, 0xC0 };
        auto *stream = static_cast<CaptureStream *>(transfer->user_data);

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

    void CaptureStream::usbTransferComplete(libusb_transfer *transfer) {
        auto *stream = static_cast<CaptureStream *>(transfer->user_data);

        switch (transfer->status) {
            case LIBUSB_TRANSFER_COMPLETED:
                // A short read means bytes were lost mid-stream: flag the gap so the parser re-syncs.
                stream->enqueueChunk(transfer->buffer, transfer->actual_length,
                                     transfer->actual_length < CHUNK_BYTES);
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

    CaptureStream::CaptureStream(libusb_device_handle *dev) : _dev{dev} {
        _frameBuffer = new uint8_t[CHUNK_BYTES * (TRANSFER_POOL_SIZE + RING_SLOTS)];
        for (int i = 0; i < RING_SLOTS; i++) {
            _ring[i].buffer = _frameBuffer + (TRANSFER_POOL_SIZE + i) * CHUNK_BYTES;
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

    CaptureStream::~CaptureStream() {
        stop();
        delete[] _frameBuffer;
    }

    void CaptureStream::start(bool alignToFrameStart) {
        if (alignToFrameStart) {
            libusb_transfer *probe = libusb_alloc_transfer(0);
            libusb_fill_bulk_transfer(probe, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer, CHUNK_BYTES,
                                      probeTransferComplete, this, 0);
            _transfers.push_back(probe);
            libusb_submit_transfer(probe);
        } else {
            // The device is mid-stream with whatever it buffered since the last session, so the
            // first transfer will not start on a frame boundary. Skip the alignment probe and let
            // the parser re-sync on the first clean SAV->EAV pair instead.
            markGap();
            queueAllFrameReads();
        }

        _readThread = std::thread(&CaptureStream::readLoop, this);
    }

    void CaptureStream::deliver(const ChunkCallback &callback) {
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
            callback(_consumeBuffer.data(), length, gapped);
        }
    }

    void CaptureStream::stop() {
        _shuttingDown = true;
        if (_readThread.joinable()) {
            _readThread.join();
        }

        if (_dumpFile != nullptr) {
            fclose(_dumpFile);
            _dumpFile = nullptr;
            printf("Wrote %llu raw chunks to dump file\n", (unsigned long long) _dumpCount);
        }
    }

    void CaptureStream::dumpChunk(const uint8_t *data, uint32_t length, bool gapped) {
        if (_dumpFile == nullptr || _dumpCount >= DUMP_MAX_CHUNKS) return;
        struct Header {
            uint32_t length;
            uint8_t gapped;
        } header{length, static_cast<uint8_t>(gapped ? 1 : 0)};
        fwrite(&header, sizeof(header), 1, _dumpFile);
        fwrite(data, 1, length, _dumpFile);
        _dumpCount++;
    }

    void CaptureStream::enqueueChunk(const uint8_t *data, uint32_t length, bool gapped) {
        if (_shuttingDown || length == 0) return;
        dumpChunk(data, length, gapped);

        std::lock_guard<std::mutex> lock(_queueMutex);
        if (length > CHUNK_BYTES) length = CHUNK_BYTES;

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

    void CaptureStream::markGap() {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _gapPending = true;
    }

    void CaptureStream::readLoop() {
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

    void CaptureStream::submitTransfer(libusb_transfer *transfer) {
        if (!_shuttingDown) {
            libusb_submit_transfer(transfer);
        } else {
            libusb_free_transfer(transfer);
            _transfers.erase(std::find(_transfers.begin(), _transfers.end(), transfer));
        }
    }

    void CaptureStream::discardTransfer(libusb_transfer *transfer) {
        _transfers.erase(std::find(_transfers.begin(), _transfers.end(), transfer));
        libusb_free_transfer(transfer);
    }

    bool CaptureStream::recordProbeAttempt() {
        return ++_probeAttempts <= MAX_PROBE_ATTEMPTS;
    }

    void CaptureStream::signalError(const char *message) {
        _errorMessage = message;
        _hasError.store(true, std::memory_order_release);
    }

    uint64_t CaptureStream::droppedChunks() const {
        return _droppedChunks.load(std::memory_order_relaxed);
    }

    uint64_t CaptureStream::transferErrors() const {
        return _transferErrors.load(std::memory_order_relaxed);
    }

    void CaptureStream::queueAllFrameReads() {
        for (int i = 0; i < TRANSFER_POOL_SIZE; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer + i * CHUNK_BYTES, CHUNK_BYTES,
                                      usbTransferComplete, this, 0);

            _transfers.push_back(transfer);
            libusb_submit_transfer(transfer);
        }
    }
}
