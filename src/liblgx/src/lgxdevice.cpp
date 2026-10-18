#include "lgxdevice.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <stdexcept>

namespace lgx2 {

    Device::Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger, ErrorSink *errorSink)
            : _stream{stream}, _videoOutput{videoOutput}, _audioOutput{audioOutput}, _logger{logger}, _errorSink{errorSink} {
        _onFrameData = [&](uint8_t *frameData, uint32_t length, bool gapped) {
            onFrameData(frameData, length, gapped);
        };

        _bufferReorderer.setSink([this](uint8_t *data, uint32_t length, bool gapped) {
            _frameBuilder.feed(data, length, gapped);
        });
        _frameBuilder.setVideoFrameCallback([this](const uint8_t *data, uint32_t byteLength) {
            produceVideoData(byteLength, data);
        });
        _frameBuilder.setAudioFrameCallback([this](const uint8_t *data, uint32_t byteLength) {
            produceAudioData(data, byteLength);
        });
    }

    bool Device::isDeviceAvailable(DeviceType device) {
        return _stream->deviceAvailable(device);
    }

    void Device::initialise(lgx2::DeviceType deviceType, lgx2::VideoScale videoScale) {
        _errorSink->catchErrors([&]() {
            if (!isDeviceAvailable(deviceType)) {
                throw std::runtime_error("Target device is not available to use - is it plugged in?");
            }

            _stream->streamSetupCommands(deviceType);

            _videoOutput->initialiseVideo(videoScale);
            _audioOutput->initialiseAudio();

            _stream->queueFrameRead(&_onFrameData);
        });
    }

    void Device::run() {
        // Checked every loop, not only when packets arrive: a disconnected input may stop
        // delivering data altogether.
        updateSignalState();

        _logger->logTimeStart("streamUpdate");
        _stream->update();
        _logger->logTimeEnd("streamUpdate", "Stream update");

        _logger->logTimeStart("videoDisplay");
        _videoOutput->display();
        _logger->logTimeEnd("videoDisplay", "Video display update");

        _logger->logTimeStart("audioOutput");
        _audioOutput->render();
        _logger->logTimeEnd("audioOutput", "Audio output render");
    }

    void Device::onFrameData(uint8_t *data, uint32_t length, bool gapped) {
        // The BufferReorderer first restores the device's 16 KiB DMA buffer order (pairs can arrive
        // swapped after an unclean stop), then hands the stream to the FrameBuilder state machine,
        // which does all marker detection, header validation and frame/audio assembly. A gap flags
        // a lost region: both re-sync, and a clean SAV->EAV pair is required before frames resume.
        _bufferReorderer.feed(data, length, gapped);

        if (_bufferReorderer.orderChanges() != _reportedOrderChanges) {
            _reportedOrderChanges = _bufferReorderer.orderChanges();
            printf("USB buffer order: %s\n", _bufferReorderer.order() == utils::BufferReorderer::Order::Swapped
                                             ? "swapped pairs (re-ordering)" : "normal");
        }
    }

    void Device::updateSignalState() {
        InputState state = _stream->inputState();
        bool present = state == InputState::Signal;
        _audioGate.update(present, utils::AudioGate::Clock::now());

        if (_audioGate.open() != _audioGateReportedOpen) {
            _audioGateReportedOpen = _audioGate.open();
            printf("Audio %s\n", _audioGateReportedOpen ? "unmuted (HDMI signal settled)" : "muted (no HDMI signal)");
        }

        if (!present && (!_videoBlanked || _blankedFor != state)) {
            _videoOutput->showNoSignal(state);
            _videoBlanked = true;
            _blankedFor = state;
        }
    }

    void Device::produceVideoData(uint32_t, const uint8_t *data) {
        // Without a signal any frame that still parses is stale or garbage: keep the output blank.
        if (!_stream->signalPresent()) return;
        _videoBlanked = false;
        _videoOutput->videoFrameAvailable((uint32_t *) data);

        auto now = std::chrono::steady_clock::now();
        _videoFrameCount++;

        if (_fpsTimestamp == std::chrono::steady_clock::time_point{}) {
            _fpsTimestamp = now;
        } else if (now - _fpsTimestamp >= std::chrono::seconds(1)) {
#ifdef LGX2_VERBOSE_STATS
            auto elapsed = std::chrono::duration<double>(now - _fpsTimestamp).count();
            // Emitted frames are always exactly VIDEO_FRAME_BYTES, so size is not a useful signal.
            // desync events (bad SAV/EAV checksum or b0/c0 mismatch), dropped chunks (consumer too
            // slow) and recovered transfer errors are what indicate garbage or load problems.
            printf("Frames: %" PRIu64 " (%.0f fps)  desync events: %u  dropped chunks: %" PRIu64
                   "  transfer errors: %" PRIu64 "\n",
                   _videoFrameCount,
                   static_cast<double>(_videoFrameCount) / elapsed,
                   _frameBuilder.desyncCount(),
                   _stream->droppedChunks(),
                   _stream->transferErrors());
#endif
            _fpsTimestamp = now;
            _videoFrameCount = 0;
        }
    }

    void Device::produceAudioData(const uint8_t *data, uint32_t byteLength) {
        // Audio packets keep arriving (often as garbage) while the input has no signal; the gate
        // ramps them to silence and keeps them silent until the signal has settled again. Silence
        // is still sent so the output keeps a steady stream instead of under-running.
        updateSignalState();
        _audioScratch.assign(data, data + byteLength);
        _audioGate.process(_audioScratch.data(), byteLength);
        _audioOutput->audioFrameAvailable(_audioScratch.data(), byteLength);
    }

    void Device::shutdown() {
        _videoOutput->shutdownVideo();
        _audioOutput->shutdownAudio();
        _stream->shutdownStream();
    }
}