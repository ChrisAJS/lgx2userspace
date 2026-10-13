#include "NullAudioOutput.h"

void NullAudioOutput::initialiseAudio() {}

void NullAudioOutput::audioFrameAvailable(const uint8_t *, uint32_t) {}

void NullAudioOutput::render() {}

void NullAudioOutput::shutdownAudio() {}
