#include "lgxdevice.h"

#include <chrono>
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <stdexcept>

namespace lgx2 {

    Device::Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger, ErrorSink *errorSink)
            : _stream{stream}, _videoOutput{videoOutput}, _audioOutput{audioOutput}, _logger{logger}, _errorSink{errorSink} {
        _onFrameData = [&](uint8_t *frameData) {
            onFrameData(frameData);
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

    void Device::onFrameData(uint8_t *data) {
        const uint32_t count = 0x1FC000 / 4;
        auto *d = reinterpret_cast<uint32_t *>(data);
        uint32_t i = 0;

        // Drain audio continuation from a previous transfer
        if (_inAudio) {
            uint32_t start = i;
            while (i < count && d[i] != utils::FrameBuilder::AUDIO_FRAME_END_MARKER) i++;
            _frameBuilder.buildAudio(data + start * 4, i - start);
            if (i < count) {
                _inAudio = 0;
                produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
                i++;
            } else {
                return;
            }
        }

        // The USB stream is raw video data with three types of delimiter:
        //   VIDEO_FRAME_END_MARKER (0xC1FFFF00) - produce the accumulated frame
        //   VIDEO_FRAME_START_MARKER (0xC0FFFF00) - protocol sync, treat as raw video data
        //   AUDIO_FRAME_START_MARKER (0x58FFFF00) - audio region follows
        // Everything that is not a delimiter is accumulated as video.
        while (i < count) {
            if (d[i] == utils::FrameBuilder::VIDEO_FRAME_END_MARKER) {
                uint32_t frameSize = _frameBuilder.videoFrameSize();
                if (frameSize >= MINIMUM_VIDEO_FRAME_SIZE) {
                    auto *frame = _frameBuilder.completeVideoFrame();
                    produceVideoData(frameSize, reinterpret_cast<uint8_t *>(frame));
                }
                // If below threshold, keep accumulating — this is a sub-frame chunk boundary.
                i++;
            } else if (d[i] == utils::FrameBuilder::AUDIO_FRAME_START_MARKER) {
                i += 2;  // skip marker + 1 padding word
                uint32_t start = i;
                while (i < count && d[i] != utils::FrameBuilder::AUDIO_FRAME_END_MARKER) i++;
                _frameBuilder.buildAudio(data + start * 4, i - start);
                if (i < count) {
                    produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
                    i++;
                } else {
                    _inAudio = 1;
                }
            } else {
                // Accumulate as video (includes VIDEO_FRAME_START_MARKER which is a protocol sync)
                uint32_t start = i;
                while (i < count
                       && d[i] != utils::FrameBuilder::VIDEO_FRAME_END_MARKER
                       && d[i] != utils::FrameBuilder::AUDIO_FRAME_START_MARKER) {
                    i++;
                }
                _frameBuilder.buildVideo(data + start * 4, i - start);
            }
        }
    }

    void Device::produceVideoData(uint32_t frameSize, uint8_t *data) {
        _videoOutput->videoFrameAvailable((uint32_t *) data);

        auto now = std::chrono::steady_clock::now();
        _videoFrameCount++;
        if (frameSize < _minVideoFrameSize) _minVideoFrameSize = frameSize;
        if (frameSize > _maxVideoFrameSize) _maxVideoFrameSize = frameSize;

        if (_fpsTimestamp == std::chrono::steady_clock::time_point{}) {
            _fpsTimestamp = now;
        } else if (now - _fpsTimestamp >= std::chrono::seconds(1)) {
            auto elapsed = std::chrono::duration<double>(now - _fpsTimestamp).count();
            printf("Frames: %" PRIu64 " (%.0f fps)  frame size min/max: %u/%u uint32s\n",
                   _videoFrameCount,
                   static_cast<double>(_videoFrameCount) / elapsed,
                   _minVideoFrameSize, _maxVideoFrameSize);
            _fpsTimestamp = now;
            _videoFrameCount = 0;
            _minVideoFrameSize = UINT32_MAX;
            _maxVideoFrameSize = 0;
        }
    }

    void Device::produceAudioData(uint8_t *data) {

        _audioOutput->audioFrameAvailable((uint32_t *) data);
    }

    void Device::shutdown() {
        _videoOutput->shutdownVideo();
        _audioOutput->shutdownAudio();
        _stream->shutdownStream();
    }
}