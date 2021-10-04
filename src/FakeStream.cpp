#include <cstdio>
#include <stdexcept>
#include <SDL.h>
#include "FakeStream.h"

fake::FakeStream::FakeStream() : _fakeFrameBuffer{new uint8_t[0x1FC000*2]} {
    _fakeFrameSource = fopen("debug.bin", "r");
    if (_fakeFrameSource == nullptr) {
        throw std::runtime_error("Failed to open debug.bin as fake source");
    }
    fseek(_fakeFrameSource, 0x1FC000, SEEK_SET);
}

void fake::FakeStream::streamSetupCommands() {
    // NOOP
}

void fake::FakeStream::queueFrameRead(std::function<void(uint8_t *, uint32_t)> *onData) {
    _onData = onData;
}

void fake::FakeStream::update() {
    fread(_fakeFrameBuffer, 1, 0x1FC000*2, _fakeFrameSource);
    if (feof(_fakeFrameSource)) {
        fclose(_fakeFrameSource);
        _fakeFrameSource = fopen("debug.bin", "r");
        fseek(_fakeFrameSource, 0x1FC000, SEEK_SET);
    }
    (*_onData)(_fakeFrameBuffer, 1920*1080*2);
    SDL_Delay(9);
}
