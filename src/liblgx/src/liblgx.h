#ifndef LGX2USERSPACE_LIBLGX_H
#define LGX2USERSPACE_LIBLGX_H

// Umbrella header for applications linking the `liblgx` target (core + usb + sdl + platform).
#include "lgxdevice.h"
#include "NullAudioOutput.h"
#include "NullVideoOutput.h"
#include "FakeUsbStream.h"
#include "SimpleErrorSink.h"
#include <usb/UsbStream.h>
#include <sdl/SdlVideoOutput.h>
#include <sdl/SdlAudioOutput.h>
#include "logging/ChronoLogger.h"
#include "logging/NOOPLogger.h"

#include <lgxplatform.h>

#endif //LGX2USERSPACE_LIBLGX_H
