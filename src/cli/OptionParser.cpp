#include <getopt.h>
#include <iostream>
#include "OptionParser.h"
#include <liblgx.h>

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

bool app::OptionParser::process(int argc, char **argv) {
    for(;;)
    {
        switch(getopt(argc, argv, "vVa:d:hxsgf"))
        {
#ifndef __MINGW32__
            case 'a':
                std::cout << "Attempting to output to Pulseaudio sink: " << optarg << std::endl;
                _audioOutput = new pulse::PulseAudioOutput(optarg);
                continue;
            case 'd':
                std::cout << "Attempting to output to V4L2Loopback device: " << optarg << std::endl;
                _videoOutput = new v4l::V4LFrameOutput(optarg);
                continue;
#endif
            case 'f':
                std::cout << "Using fake USB stream - reading data from 'dump.bin'" << std::endl;
                _stream = new FakeUsbStream();
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
                continue;
#endif
            case 's':
                _videoOutput = new NullVideoOutput();
                continue;
            case 'g':
                _audioOutput = new NullAudioOutput();
                continue;
            case 'h':
            default :
                std::cout << argv[0] <<
                    " usage:\n"
                    "\t-v\tPrint diagnostics information summary at end of execution, useful when submitting bugs\n"
                    "\t-V\tPrint diagnostic information during execution\n"
                    "\t-d V4L2LoopbackDevice\tSpecify the V4L2Loopback device to output video to (e.g. /dev/video99)\n\n"
#ifdef GC550_SUPPORT
                    "\t-x Use LGX (GC550) device specifically\n"
#endif
                    "\t-a Pulseaudio sink\tOutput audio to a Pulseaudio sink (useful when outputting video to V4L2Loopback)\n"
                    "\t-s Output only sound\n"
                    "\t-g Output video only\n"
                    "\t-f Use a fake USB stream containing unprocessed frames from a dump.bin file\n";
                return false;
            case -1:
                break;
        }

        break;
    }

    return true;
}
