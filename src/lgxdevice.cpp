#include <cstdio>
#include "lgxdevice.h"

#include <chrono>
#include <unistd.h>

namespace lgx2 {

    Device::Device(Stream *stream, VideoOutput *videoOutput, AudioOutput *audioOutput, Logger *logger)
            : _stream{stream}, _videoOutput{videoOutput}, _audioOutput{audioOutput}, _logger{logger} {
        _onFrameData = [&](uint8_t *frameData) {
            onFrameData(frameData);
        };
    }

    void Device::initialise() {
        _videoOutput->initialiseVideo();
        _audioOutput->initialiseAudio();

        _stream->streamSetupCommands();
        _stream->queueFrameRead(&_onFrameData);
    }

    void Device::run() {
        ensureMinimumDuration([&]() {
            _logger->logTimeStart("streamUpdate");
            _stream->update();
            _logger->logTimeEnd("streamUpdate", "Stream update");

            _logger->logTimeStart("videoDisplay");
            _videoOutput->display();
            _logger->logTimeEnd("videoDisplay", "Video display update");

            _logger->logTimeStart("audioOutput");
            _audioOutput->render();
            _logger->logTimeEnd("audioOutput", "Audio output render");
        });
    }

    void Device::onFrameData(uint8_t *data) {
        static int inVideo = 0;
        static int inAudio = 0;

        auto *yuvData = (uint32_t *) data;

        for (uint32_t i = 0; i < 0x1FC000 / 4; i++) {

            if (i > 2 && yuvData[i - 2] == 0xC0FFFF00) {
                inVideo = 1;
            }

            if (i > 2 && yuvData[i - 2] == 0x58FFFF00) {
                inAudio = 1;
            }

            if (yuvData[i] == 0xC1FFFF00) {
                inVideo = 0;
                produceVideoData(reinterpret_cast<uint8_t *>(_frameBuilder.completeVideoFrame()));
            }

            if (inAudio && yuvData[i] == 0xAA5555AA) {
                inAudio = 0;
                produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
            }

            if (inVideo) {
                _frameBuilder.buildVideo((uint8_t *) (yuvData + i), 1);
            } else if (inAudio) {
                _frameBuilder.buildAudio((uint8_t *) (yuvData + i), 1);
            }
        }
    }

    void Device::produceVideoData(uint8_t *data) {

        _videoOutput->videoFrameAvailable((uint32_t *) data);
    }

    void Device::produceAudioData(uint8_t *data) {

        _audioOutput->audioFrameAvailable((uint32_t *) data);
    }

    void Device::shutdown() {
        _videoOutput->shutdownVideo();
        _audioOutput->shutdownAudio();
        _stream->shutdownStream();
    }

    void Device::ensureMinimumDuration(const std::function<void()> &action) {
        auto start = std::chrono::steady_clock::now();
        action();
        auto end = std::chrono::steady_clock::now();
        long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        if (timeTaken < 8333) {
            usleep(8333 - timeTaken);
        }
    }
}