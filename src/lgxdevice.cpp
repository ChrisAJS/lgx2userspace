#include "lgxdevice.h"

namespace lgx2 {

    Device::Device(Stream *stream, FrameOutput *frameOutput) : _stream{stream}, _frameOutput{frameOutput} {
        _onFrameData = [&](uint8_t *frameData) {
            onFrameData(frameData);
        };
    }

    void Device::initialise() {
        _stream->streamSetupCommands();
        _stream->queueFrameRead(&_onFrameData);
    }

    void Device::run() {
        _stream->update();

        _frameOutput->render();
    }

    void Device::onFrameData(uint8_t *data) {
        static int inVideo = 0;
        static int posInImage = 0;
        static int inAudio = 0;
        auto *yuvData = (uint32_t*)data;
        uint32_t startPos = 0;
        if (!inVideo) {
            for(uint32_t i = 0; i < 0x1FC000/4; i++) {
                if (yuvData[i] == 0xC0FFFF00) {
                    inVideo = 1;
                    startPos = i+2;
                    posInImage = 0;
                    break;
                } else if (i > 2 && yuvData[i - 2] == 0x58FFFF00) {
                    inAudio = 1;
                    _frameBuilder.buildAudio((uint8_t *) (yuvData + i), 1);
                } else if (inAudio  && yuvData[i] == 0xAA5555AA) {
                    inAudio = 0;
                    produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
                } else if (inAudio) {
                    _frameBuilder.buildAudio((uint8_t *) (yuvData + i), 1);
                }
            }
        }

        if (!inVideo) {
            return;
        }

        for (uint32_t i = startPos; i<0x1FC000/4; i++) {
            if (posInImage > 1920 * 1080) {
                inVideo = 0;
                posInImage = 0;
                produceVideoData(reinterpret_cast<uint8_t *>(_frameBuilder.completeVideoFrame()));
            }
            if (yuvData[i] == 0xC1FFFF00) {
                inVideo = 0;
                posInImage = 0;
                produceVideoData(reinterpret_cast<uint8_t *>(_frameBuilder.completeVideoFrame()));

            }
            if (!inVideo && yuvData[i - 2] == 0x58FFFF00) {
                inAudio = 1;
                _frameBuilder.buildAudio((uint8_t *) (yuvData + i), 1);
            } else if (yuvData[i] == 0xAA5555AA) {
                inAudio = 0;
                produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
                break;
            }

            if (inVideo) {
                _frameBuilder.buildVideo((uint8_t *) (yuvData + i), 1);
            } else if (inAudio) {
                _frameBuilder.buildAudio((uint8_t *) (yuvData + i), 1);
            }
        }
    }

    void Device::produceVideoData(uint8_t *data) {

        _frameOutput->videoFrameAvailable((uint32_t *) data);
    }

    void Device::produceAudioData(uint8_t *data) {

        _frameOutput->audioFrameAvailable((uint32_t *) data);
    }
}