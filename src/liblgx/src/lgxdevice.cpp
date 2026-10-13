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
        // All marker detection, header validation and frame/audio assembly lives in the
        // FrameBuilder state machine. A gap flags a lost region: it resets parser state
        // and requires a clean SAV->EAV pair before frames are emitted again.
        _frameBuilder.feed(data, length, gapped);
    }

    void Device::produceVideoData(uint32_t, const uint8_t *data) {
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
        _audioOutput->audioFrameAvailable(data, byteLength);
    }

    void Device::shutdown() {
        _videoOutput->shutdownVideo();
        _audioOutput->shutdownAudio();
        _stream->shutdownStream();
    }
}