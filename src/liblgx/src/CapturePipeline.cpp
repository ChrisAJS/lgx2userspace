#include "CapturePipeline.h"

#include <cstdio>

namespace lgx2 {

    CapturePipeline::CapturePipeline(VideoOutput *video, AudioOutput *audio, Options options)
            : _video{video}, _audio{audio}, _options{options} {
        _bufferReorderer.setSink([this](uint8_t *data, uint32_t length, bool gapped) {
            _frameBuilder.feed(data, length, gapped);
        });
        _frameBuilder.setVideoFrameCallback([this](const uint8_t *data, uint32_t) {
            onVideoFrame(data);
        });
        _frameBuilder.setAudioFrameCallback([this](const uint8_t *data, uint32_t byteLength) {
            onAudioPacket(data, byteLength);
        });
    }

    void CapturePipeline::feed(uint8_t *chunk, uint32_t length, bool gapped) {
        // The BufferReorderer first restores the device's 16 KiB DMA buffer order (pairs can arrive
        // swapped after an unclean stop), then hands the stream to the FrameBuilder state machine,
        // which does all marker detection, header validation and frame/audio assembly. A gap flags
        // a lost region: both re-sync, and a clean SAV->EAV pair is required before frames resume.
        _bufferReorderer.feed(chunk, length, gapped);

        if (_bufferReorderer.orderChanges() != _reportedOrderChanges) {
            _reportedOrderChanges = _bufferReorderer.orderChanges();
            printf("USB buffer order: %s\n", _bufferReorderer.order() == utils::BufferReorderer::Order::Swapped
                                             ? "swapped pairs (re-ordering)" : "normal");
        }
    }

    void CapturePipeline::setInputState(InputState state, Clock::time_point now) {
        _inputState = state;
        bool present = state == InputState::Signal;
        _audioGate.update(present, now);

        if (_audioGate.open() != _audioGateReportedOpen) {
            _audioGateReportedOpen = _audioGate.open();
            printf("Audio %s\n", _audioGateReportedOpen ? "unmuted (HDMI signal settled)" : "muted (no HDMI signal)");
        }

        if (!present && (!_videoBlanked || _blankedFor != state)) {
            if (_video) _video->showNoSignal(state);
            _videoBlanked = true;
            _blankedFor = state;
            _lastKeepAlive = {}; // show the new card straight away
        }
    }

    void CapturePipeline::tick(Clock::time_point now) {
        if (!_options.keepAliveFrames || _video == nullptr || _inputState == InputState::Signal) return;
        if (_lastKeepAlive != Clock::time_point{} && now - _lastKeepAlive < _options.keepAliveInterval) return;
        _lastKeepAlive = now;
        _keepAliveFramesEmitted++;
        _video->videoFrameAvailable(_card.frame(_inputState));
    }

    void CapturePipeline::onVideoFrame(const uint8_t *data) {
        // Without a signal any frame that still parses is stale or garbage: keep the output blank.
        if (_inputState != InputState::Signal) return;
        _videoBlanked = false;
        _videoFramesEmitted++;
        if (_video) _video->videoFrameAvailable(const_cast<uint32_t *>(reinterpret_cast<const uint32_t *>(data)));
    }

    void CapturePipeline::onAudioPacket(const uint8_t *data, uint32_t byteLength) {
        // Audio packets keep arriving (often as garbage) while the input has no signal; the gate
        // ramps them to silence and keeps them silent until the signal has settled again. Silence
        // is still sent so the output keeps a steady stream instead of under-running.
        if (_audio == nullptr) return;
        _audioScratch.assign(data, data + byteLength);
        _audioGate.process(_audioScratch.data(), byteLength);
        _audio->audioFrameAvailable(_audioScratch.data(), byteLength);
    }

    uint64_t CapturePipeline::videoFramesEmitted() const {
        return _videoFramesEmitted;
    }

    uint64_t CapturePipeline::keepAliveFramesEmitted() const {
        return _keepAliveFramesEmitted;
    }

    uint32_t CapturePipeline::desyncCount() const {
        return _frameBuilder.desyncCount();
    }
}
