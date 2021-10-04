#include <cstdio>
#include "lgxdevice.h"
#include <thread>

namespace lgx2 {

    Device::Device(Stream *stream, FrameOutput *frameOutput) : _stream{stream}, _frameOutput{frameOutput} {
        _onFrameData = [&](uint8_t *frameData, uint32_t frameSize) {
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

//    void Device::onFrameData(uint8_t *data) {
//        static const uint32_t frameSizeBytes = 0x1FC000;
//        static const uint32_t frameStartMarker = 0xC0FFFF00;
//        static const uint32_t audioFrameStartMarker = 0x58FFFF00;
//
//        auto *frameData = (uint32_t *) data;
//
//        if (_frameBuilder.buildingVideoFrame()) {
//            uint32_t blocksUntilComplete = _frameBuilder.blocksUntilVideoComplete();
//            uint32_t audioBufferBytes = frameSizeBytes - blocksUntilComplete*4;
//
//            uint8_t *remainingBuffer = _frameBuilder.buildVideo(data, frameSizeBytes);
//            for (int i = 0; i < audioBufferBytes/4; i++) {
//                if (*(uint32_t*)remainingBuffer == audioFrameStartMarker) {
//                    _frameBuilder.buildAudio(remainingBuffer + i * 4 + 8, 800*4);
//                }
//            }
//        } else {
//            for (int i = 0; i < frameSizeBytes / 4; i++) {
//
//                if (frameData[i] == frameStartMarker) {
//                    _frameBuilder.buildVideo(data + i * 4 + 8, frameSizeBytes-(i*4 + 8));
//                    if (i + 1920*1080*2 < frameSizeBytes/4) {
//                        i += 1920*1080*2;
//                        // Skip ahead of the image data to process any remaining audio data
//                    } else {
//                        break;
//                    }
//                }
//
//                if (frameData[i] == audioFrameStartMarker) {
//                    _frameBuilder.buildAudio(data + i * 4 + 8, 800*4);
//                }
//            }
//        }
//
//        uint32_t *completeVideoFrame = _frameBuilder.completeVideoFrame();
//        if (completeVideoFrame) {
//            produceVideoData(reinterpret_cast<uint8_t *>(completeVideoFrame));
//        }
//
//        uint32_t *completeAudioFrame = _frameBuilder.completeAudioFrame();
//        if (completeAudioFrame) {
//            produceAudioData(reinterpret_cast<uint8_t *>(completeAudioFrame));
//        }
//    }

    void Device::onFrameData(uint8_t *data) {
        static int inFrame = 0;
        static int posInImage = 0;
        static int inAudio = 0;
        uint32_t *yuvData = (uint32_t*)data;
        uint32_t startPos = 0;
        if (!inFrame) {
            for(uint32_t i = 0; i < 0x1FC000/4; i++) {
                if (yuvData[i] == 0xC0FFFF00) {
                    inFrame = 1;
                    startPos = i+2;
                    posInImage = 0;
                    break;
                }
            }

            if (!inFrame) {
//            printf("Failed to find frame beginning :(\n");
            }
        } else {
            // startPos = 0 so we're fine since we're already in a frame
        }

        if (!inFrame) {
            return;
        }

        for (uint32_t i = startPos; i<0x1FC000/4; i++) {
            if (posInImage > 1920 * 1080) {
                inFrame = 0;
                posInImage = 0;
                produceVideoData(reinterpret_cast<uint8_t *>(_frameBuilder.completeVideoFrame()));
                //onFrameData(buffer + i*4, readLength-i*4);

            }
            if (yuvData[i] == 0xC1FFFF00) {
//            printf("End of frame found - %d\n", posInImage);
                inFrame = 0;
                posInImage = 0;
                produceVideoData(reinterpret_cast<uint8_t *>(_frameBuilder.completeVideoFrame()));

            }
            if (!inFrame && yuvData[i-2] == 0x58FFFF00) {
                inAudio = 1;
            } else if (yuvData[i] == 0xAA5555AA) {
                inAudio = 0;
                produceAudioData(reinterpret_cast<uint8_t *>(_frameBuilder.completeAudioFrame()));
                break;
            }

            if (inFrame) {
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