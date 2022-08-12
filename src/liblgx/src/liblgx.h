#ifndef LGX2USERSPACE_LIBLGX_H
#define LGX2USERSPACE_LIBLGX_H
#include "config.h"


#include "lgxdevice.h"
#include "usb/UsbStream.h"
#ifdef USE_SDL
#include "sdl/SdlFrameOutput.h"
#endif
#ifdef USE_SFML
#include "sfml/SFMLFrameOutput.h"
#endif
#include "NullAudioOutput.h"
#include "NullVideoOutput.h"
#include "logging/ChronoLogger.h"
#include "logging/NOOPLogger.h"

#ifndef __APPLE__
#ifndef __MINGW32__
#  include "pulseaudio/PulseAudioOutput.h"
#  include "v4l/V4LFrameOutput.h"
#endif
#endif

#endif //LGX2USERSPACE_LIBLGX_H
