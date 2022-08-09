#ifndef LGX2USERSPACE_LIBLGX_H
#define LGX2USERSPACE_LIBLGX_H

#include <SDL2/SDL.h>
#include "lgxdevice.h"
#include "usb/UsbStream.h"
#include "sdl/SdlFrameOutput.h"
#include "NullAudioOutput.h"
#include "NullVideoOutput.h"
#include "logging/ChronoLogger.h"
#include "logging/NOOPLogger.h"

#ifndef __MINGW32__
#  include "pulseaudio/PulseAudioOutput.h"
#  include "v4l/V4LFrameOutput.h"
#endif

#endif //LGX2USERSPACE_LIBLGX_H
