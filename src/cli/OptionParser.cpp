#include <getopt.h>
#include <iostream>
#include "OptionParser.h"

lgx2::VideoOutput *app::OptionParser::videoOutput() {
    return _videoOutput;
}

lgx2::AudioOutput *app::OptionParser::audioOutput() {
    return _audioOutput;
}

lgx2::Logger *app::OptionParser::logger() {
    return _logger;
}

lgx2::Stream *app::OptionParser::stream() {
    return _stream;
}

lgx2::DeviceType app::OptionParser::deviceType() {
    return _deviceType;
}

lgx2::VideoScale app::OptionParser::scale() {
    return _scale;
}

bool app::OptionParser::direct() {
    return _direct;
}

const std::string &app::OptionParser::inputDevice() {
    return _inputDevice;
}

bool app::OptionParser::process(int argc, char **argv) {
    for(;;)
    {
        switch(getopt(argc, argv, "vVd:hxsgfS:Di:"))
        {
#ifndef __MINGW32__
#ifndef __APPLE__
            case 'd':
                std::cout << "Attempting to output to V4L2Loopback device: " << optarg << std::endl;
                _videoOutput = new v4l::V4LFrameOutput(optarg);
                _direct = true;
                continue;
            case 'i':
                _inputDevice = optarg;
                continue;
#endif
#endif
            case 'D':
                _direct = true;
                continue;
            case 'f':
                std::cout << "Using fake USB stream - reading data from 'dump.bin'" << std::endl;
                _stream = new FakeUsbStream();
                _direct = true;
                continue;
            case 'v':
                std::cout <<"Logging diagnostics information at end of execution" << std::endl;
                _logger = new ChronoLogger(true);
                continue;
            case 'V':
                std::cout <<"Logging diagnostics information - with output during execution " << std::endl;
                _logger = new ChronoLogger(false);
                continue;
#ifdef GC550_SUPPORT
            case 'x':
                std::cout << "Using the LGX GC550 support" << std::endl;
                _deviceType = lgx2::DeviceType::LGX;
                _direct = true;
                continue;
#endif
            case 's':
                _videoOutput = new NullVideoOutput();
                continue;
            case 'g':
                _audioOutput = new NullAudioOutput();
                continue;
            case 'S':
                std::cout << "Setting output scaling to: 1/" << optarg << std::endl;
                if (std::string("2") == optarg) {
                    _scale = lgx2::VideoScale::Half;
                } else if (std::string("4") == optarg) {
                    _scale = lgx2::VideoScale::Quarter;
                }
                continue;
            case 'h':
            default :
                std::cout << argv[0] <<
                    " usage:\n"
#ifdef __linux__
                    "By default, shows the video and audio lgx2d is capturing (start lgx2d first).\n"
                    "\t-D\tCapture from the LGX2 directly instead (lgx2d must not be running)\n"
                    "\t-i V4L2Device\tlgx2d's V4L2 device to view (default: find the device named LGX2)\n"
#endif
                    "\t-h\tPrint this usage message\n"
                    "\t-v\tPrint diagnostics information summary at end of execution, useful when submitting bugs\n"
                    "\t-V\tPrint diagnostic information during execution\n"
#ifndef __MINGW32__
#ifndef __APPLE__
                    "\t-d V4L2LoopbackDevice\tWhen capturing directly, output video to this V4L2Loopback device (e.g. /dev/video99)\n"
#endif
#endif
#ifdef GC550_SUPPORT
                    "\t-x Use LGX (GC550) device specifically\n"
#endif
                    "\t-s Output only sound\n"
                    "\t-g Output video only\n"
                    "\t-f Use a fake USB stream containing unprocessed frames from a dump.bin file\n"
                    "\t-S SCALE\tSpecify the output scaling (1, 2, 4)\n";
                return false;
            case -1:
                break;
        }

        break;
    }

    return true;
}
