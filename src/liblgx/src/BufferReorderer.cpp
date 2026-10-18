#include <cstring>
#include "BufferReorderer.h"

namespace utils {

    namespace {
        // Same header checks as FrameBuilder (SAV 4 bytes, EAV 5 bytes after the marker).
        bool savHeaderValid(const uint8_t *b) {
            return b[3] == static_cast<uint8_t>(b[0] + b[1] + b[2] - 0x40);
        }

        bool eavHeaderValid(const uint8_t *c) {
            return c[4] == static_cast<uint8_t>(c[0] + c[1] + c[2] + c[3] - 0x3F);
        }

        // Longest marker + header the scanner needs in view (EAV: 4 + 5).
        constexpr uint32_t MARKER_SPAN = 9;
    }

    void BufferReorderer::setSink(Sink sink) {
        _sink = std::move(sink);
    }

    BufferReorderer::Order BufferReorderer::order() const {
        return _order;
    }

    uint32_t BufferReorderer::orderChanges() const {
        return _orderChanges;
    }

    const uint8_t *BufferReorderer::at(uint64_t offset) const {
        return _pipe.data() + (offset - _pipeStart);
    }

    void BufferReorderer::reset() {
        _pipe.clear();
        _pipeStart = _inOffset;
        _outOffset = _inOffset;
        _scanOffset = _inOffset;
        _pendingSav = false;
        _observations.clear();
        _order = Order::Unknown;
        _candidateCount = 0;
        _gapPending = true;
    }

    void BufferReorderer::observe(uint64_t savOffset, Order order) {
        _observations.push_back({savOffset, order});
    }

    void BufferReorderer::scan() {
        if (_inOffset < MARKER_SPAN) return;
        const uint64_t limit = _inOffset - MARKER_SPAN + 1; // marker + header fully in view
        while (_scanOffset < limit) {
            // A SAV with no late EAV inside the window: normal order.
            if (_pendingSav && _scanOffset >= _pendingSavOffset + SWAP_WINDOW) {
                observe(_pendingSavOffset, Order::Normal);
                _pendingSav = false;
            }

            // Markers start with 00, which is rare in video: jump between zero bytes, but not
            // past a pending SAV's deadline.
            uint64_t stop = limit;
            if (_pendingSav && _pendingSavOffset + SWAP_WINDOW < stop) stop = _pendingSavOffset + SWAP_WINDOW;
            const uint8_t *base = at(_scanOffset);
            const void *zero = memchr(base, 0x00, stop - _scanOffset);
            if (zero == nullptr) {
                _scanOffset = stop;
                continue;
            }
            _scanOffset += static_cast<const uint8_t *>(zero) - base;

            const uint8_t *p = at(_scanOffset);
            _scanOffset++;
            if (p[1] != 0xFF || p[2] != 0xFF) continue;

            if (p[3] == 0xC0 && savHeaderValid(p + 4)) {
                if (_pendingSav) observe(_pendingSavOffset, Order::Normal);
                _pendingSav = true;
                _pendingSavOffset = _scanOffset - 1;
                _pendingSavB0 = p[4];
            } else if (p[3] == 0xC1 && eavHeaderValid(p + 4) && _pendingSav) {
                // An EAV this close to a SAV either belongs to an earlier frame (swapped pairs) or
                // ends a tiny startup partial (normal order). The frame counter is not strictly +1
                // (it goes fe ff 01, skipping 00), so test for "different", not "b0 - 1".
                bool late = p[4] != _pendingSavB0;
                observe(_pendingSavOffset, late ? Order::Swapped : Order::Normal);
                _pendingSav = false;
            }
        }
        if (_pendingSav && _scanOffset >= _pendingSavOffset + SWAP_WINDOW) {
            observe(_pendingSavOffset, Order::Normal);
            _pendingSav = false;
        }
    }

    void BufferReorderer::flushOut() {
        if (_out.empty() || !_sink) return;
        bool gap = _gapPending;
        _gapPending = false;
        _sink(_out.data(), static_cast<uint32_t>(_out.size()), gap);
        _out.clear();
    }

    bool BufferReorderer::agrees(const Observation &observation, Order order, uint64_t pairOrigin) {
        if (observation.order != order) return false;
        // A swapped SAV must also sit on the pair grid; one off the grid means the phase moved.
        return order != Order::Swapped || (observation.savOffset - pairOrigin) % PAIR_BYTES == 0;
    }

    void BufferReorderer::applyObservation(const Observation &observation) {
        if (_order != Order::Unknown) {
            if (agrees(observation, _order, _pairOrigin)) {
                _candidateCount = 0;
                return;
            }
            // Once an order is established, change it only when two consecutive SAVs agree, so a
            // stray marker pattern (e.g. inside audio data) cannot flip it.
            if (_candidateCount > 0 && agrees(observation, _candidateOrder, _candidateOrigin)) {
                _candidateCount++;
            } else {
                _candidateOrder = observation.order;
                _candidateOrigin = observation.savOffset;
                _candidateCount = 1;
            }
            if (_candidateCount < 2) return;
            _candidateCount = 0;
        }

        // Output so far was in the old order; the parser must re-sync from here.
        flushOut();
        _order = observation.order;
        _pairOrigin = observation.savOffset;
        _gapPending = true;
        _orderChanges++;
    }

    void BufferReorderer::emit(uint64_t limit) {
        // limit: every SAV before it has been seen and decided, so bytes before it can be ordered.
        while (_outOffset < limit) {
            uint64_t nextSav = _observations.empty() ? UINT64_MAX : _observations.front().savOffset;
            if (nextSav == _outOffset) {
                applyObservation(_observations.front());
                _observations.pop_front();
                continue;
            }

            uint64_t end = nextSav < limit ? nextSav : limit;
            switch (_order) {
                case Order::Unknown:
                    // Nothing decided yet: drop up to the first decided SAV.
                    _outOffset = end;
                    break;
                case Order::Normal:
                    _out.insert(_out.end(), at(_outOffset), at(end));
                    _outOffset = end;
                    break;
                case Order::Swapped: {
                    if (_outOffset + PAIR_BYTES > limit) return; // wait for the rest of the pair
                    if (nextSav < _outOffset + PAIR_BYTES) {
                        // A SAV inside this pair. If it really changes the order (the phase moved),
                        // drop the partial pair and re-anchor at it; a stray one is just ignored.
                        Observation observation = _observations.front();
                        _observations.pop_front();
                        uint32_t changesBefore = _orderChanges;
                        applyObservation(observation);
                        if (_orderChanges != changesBefore) _outOffset = observation.savOffset;
                        break;
                    }
                    const uint8_t *pair = at(_outOffset);
                    _out.insert(_out.end(), pair + DMA_BUFFER_BYTES, pair + PAIR_BYTES);
                    _out.insert(_out.end(), pair, pair + DMA_BUFFER_BYTES);
                    _outOffset += PAIR_BYTES;
                    break;
                }
            }
        }
    }

    void BufferReorderer::feed(const uint8_t *chunk, uint32_t length, bool gapped) {
        if (gapped) reset();
        if (length == 0) return;

        _pipe.insert(_pipe.end(), chunk, chunk + length);
        _inOffset += length;
        scan();

        // Hold back enough that every SAV before the limit has its verdict (window + marker span).
        const uint64_t holdBack = SWAP_WINDOW + MARKER_SPAN;
        _out.clear();
        if (_inOffset > holdBack) {
            emit(_inOffset - holdBack);
            _pipe.erase(_pipe.begin(), _pipe.begin() + static_cast<std::ptrdiff_t>(_outOffset - _pipeStart));
            _pipeStart = _outOffset;
        }

        flushOut();
    }
}
