#include "CaptureStream.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

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

        if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
            stream->signalError("USB capture device was disconnected");
            stream->discardTransfer(transfer);
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
        // the marker something is genuinely wrong — stop rather than accumulate garbage.
        if (!stream->recordProbeAttempt()) {
            stream->signalError("Device did not start sending at a frame boundary after multiple attempts — try replugging");
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
                stream->signalError("USB capture device was disconnected");
                stream->discardTransfer(transfer);
                return;
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
            std::lock_guard<std::mutex> lock(_transfersMutex);
            libusb_transfer *probe = libusb_alloc_transfer(0);
            libusb_fill_bulk_transfer(probe, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer, CHUNK_BYTES,
                                      probeTransferComplete, this, 0);
            addTransferLocked(probe);
        } else {
            // The device is mid-stream with whatever it buffered since the last session, so the
            // first transfer will not start on a frame boundary. Skip the alignment probe and let
            // the parser re-sync on the first clean SAV->EAV pair instead.
            markGap();
            queueAllFrameReads();
        }
    }

    void CaptureStream::deliver(const ChunkCallback &callback) {
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
        {
            std::unique_lock<std::mutex> lock(_transfersMutex);
            _shuttingDown = true;
            for (auto *transfer : _transfers) {
                libusb_cancel_transfer(transfer);
            }
            // Completions (on the event thread) free each cancelled transfer and remove it.
            if (!_transfersDrained.wait_for(lock, STOP_TIMEOUT, [this] { return _transfers.empty(); })) {
                fprintf(stderr, "%zu USB transfers did not complete after cancellation\n", _transfers.size());
            }
        }

        if (_dumpFile != nullptr) {
            fclose(_dumpFile);
            _dumpFile = nullptr;
            printf("Wrote %llu raw chunks to dump file\n", (unsigned long long) _dumpCount);
        }
    }

    bool CaptureStream::failed() const {
        return _hasError.load(std::memory_order_acquire);
    }

    std::string CaptureStream::failure() const {
        std::lock_guard<std::mutex> lock(_errorMutex);
        return _errorMessage;
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

    void CaptureStream::addTransferLocked(libusb_transfer *transfer) {
        int res = libusb_submit_transfer(transfer);
        if (res != LIBUSB_SUCCESS) {
            libusb_free_transfer(transfer);
            if (res == LIBUSB_ERROR_NO_DEVICE) signalError("USB capture device was disconnected");
            return;
        }
        _transfers.push_back(transfer);
    }

    void CaptureStream::removeTransferLocked(libusb_transfer *transfer) {
        auto it = std::find(_transfers.begin(), _transfers.end(), transfer);
        if (it != _transfers.end()) _transfers.erase(it);
        if (_transfers.empty()) _transfersDrained.notify_all();
    }

    void CaptureStream::submitTransfer(libusb_transfer *transfer) {
        std::lock_guard<std::mutex> lock(_transfersMutex);
        if (!_shuttingDown) {
            int res = libusb_submit_transfer(transfer);
            if (res == LIBUSB_SUCCESS) return;
            // Could not resubmit (e.g. the device is gone): drop it so stop() is not left waiting.
            if (res == LIBUSB_ERROR_NO_DEVICE) signalError("USB capture device was disconnected");
        }
        removeTransferLocked(transfer);
        libusb_free_transfer(transfer);
    }

    void CaptureStream::discardTransfer(libusb_transfer *transfer) {
        std::lock_guard<std::mutex> lock(_transfersMutex);
        removeTransferLocked(transfer);
        libusb_free_transfer(transfer);
    }

    bool CaptureStream::recordProbeAttempt() {
        return ++_probeAttempts <= MAX_PROBE_ATTEMPTS;
    }

    void CaptureStream::signalError(const char *message) {
        {
            // Every in-flight transfer reports a disconnect: keep and print only the first.
            std::lock_guard<std::mutex> lock(_errorMutex);
            if (_hasError) return;
            _errorMessage = message;
        }
        fprintf(stderr, "%s\n", message);
        _hasError.store(true, std::memory_order_release);
    }

    uint64_t CaptureStream::droppedChunks() const {
        return _droppedChunks.load(std::memory_order_relaxed);
    }

    uint64_t CaptureStream::transferErrors() const {
        return _transferErrors.load(std::memory_order_relaxed);
    }

    void CaptureStream::queueAllFrameReads() {
        std::lock_guard<std::mutex> lock(_transfersMutex);
        if (_shuttingDown) return;
        for (int i = 0; i < TRANSFER_POOL_SIZE; i++) {
            libusb_transfer *transfer = libusb_alloc_transfer(0);

            libusb_fill_bulk_transfer(transfer, _dev, LIBUSB_ENDPOINT_IN | 0x03,
                                      _frameBuffer + i * CHUNK_BYTES, CHUNK_BYTES,
                                      usbTransferComplete, this, 0);

            addTransferLocked(transfer);
        }
    }
}
