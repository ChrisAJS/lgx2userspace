#include "NullVideoOutput.h"

void NullVideoOutput::initialiseVideo(lgx2::VideoScale) {}

void NullVideoOutput::videoFrameAvailable(uint32_t *) {}

void NullVideoOutput::display() {}

void NullVideoOutput::shutdownVideo() {}
