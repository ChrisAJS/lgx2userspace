#include "FakeUsbStream.h"

FILE *dump = nullptr;
uint8_t *frameData;

FakeUsbStream::FakeUsbStream() {
    dump = fopen("dump.bin", "r");
    frameData = new uint8_t[0x1FC000];
}

bool FakeUsbStream::deviceAvailable(lgx2::DeviceType deviceType) {
    return true;
}

void FakeUsbStream::streamSetupCommands(lgx2::DeviceType deviceType) {
    // No-op
}

void FakeUsbStream::queueFrameRead(std::function<void(uint8_t *)> *onData) {
    _onFrameDataCallback = onData;
}

void FakeUsbStream::update() {
    if (fread(frameData, 1, 0x1FC000, dump) != 0x1FC000) {
        rewind(dump);
    }
    (*_onFrameDataCallback)(frameData);
}

void FakeUsbStream::shutdownStream() {
    fclose(dump);
    delete[] frameData;
}
