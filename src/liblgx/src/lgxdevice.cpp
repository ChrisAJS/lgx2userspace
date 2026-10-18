#include "lgxdevice.h"

#include <cinttypes>
#include <cstdio>
#include <stdexcept>

namespace lgx2 {

    Device::Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger, ErrorSink *errorSink)
            : _stream{stream}, _videoOutput{videoOutput}, _audioOutput{audioOutput}, _logger{logger}, _errorSink{errorSink},
              _pipeline{videoOutput, audioOutput, CapturePipeline::Options{}} {
        _onFrameData = [this](uint8_t *frameData, uint32_t length, bool gapped) {
            _pipeline.feed(frameData, length, gapped);
        };
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
        auto now = std::chrono::steady_clock::now();
        // Checked every loop, not only when packets arrive: a disconnected input may stop
        // delivering data altogether.
        _pipeline.setInputState(_stream->inputState(), now);
        _pipeline.tick(now);

        _logger->logTimeStart("streamUpdate");
        _stream->update();
        _logger->logTimeEnd("streamUpdate", "Stream update");

        _logger->logTimeStart("videoDisplay");
        _videoOutput->display();
        _logger->logTimeEnd("videoDisplay", "Video display update");

        _logger->logTimeStart("audioOutput");
        _audioOutput->render();
        _logger->logTimeEnd("audioOutput", "Audio output render");

        reportStats(now);
    }

    void Device::reportStats(std::chrono::steady_clock::time_point now) {
        if (_statsTimestamp == std::chrono::steady_clock::time_point{}) {
            _statsTimestamp = now;
            _statsFrames = _pipeline.videoFramesEmitted();
            return;
        }
        if (now - _statsTimestamp < std::chrono::seconds(1)) return;
#ifdef LGX2_VERBOSE_STATS
        auto elapsed = std::chrono::duration<double>(now - _statsTimestamp).count();
        uint64_t frames = _pipeline.videoFramesEmitted() - _statsFrames;
        // Emitted frames are always exactly one 1080p frame, so size is not a useful signal.
        // desync events (bad SAV/EAV checksum or b0/c0 mismatch), dropped chunks (consumer too
        // slow) and recovered transfer errors are what indicate garbage or load problems.
        printf("Frames: %" PRIu64 " (%.0f fps)  desync events: %u  dropped chunks: %" PRIu64
               "  transfer errors: %" PRIu64 "\n",
               frames, static_cast<double>(frames) / elapsed, _pipeline.desyncCount(),
               _stream->droppedChunks(), _stream->transferErrors());
#endif
        _statsTimestamp = now;
        _statsFrames = _pipeline.videoFramesEmitted();
    }

    void Device::shutdown() {
        _videoOutput->shutdownVideo();
        _audioOutput->shutdownAudio();
        _stream->shutdownStream();
    }
}
