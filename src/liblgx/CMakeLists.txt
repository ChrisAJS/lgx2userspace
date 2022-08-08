cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

set(LIBLGX_BASE_SOURCES
        liblgx.h
        lgxdevice.cpp
        lgxdevice.h
        bootstrap/commanddata_lgx2.cpp
        bootstrap/commanddata_lgx2.h
        bootstrap/commanddata_lgx.cpp
        bootstrap/commanddata_lgx.h
        usb/UsbStream.cpp
        usb/UsbStream.h
        FrameBuilder.cpp
        FrameBuilder.h
        NullVideoOutput.cpp
        NullVideoOutput.h
        NullAudioOutput.cpp
        NullAudioOutput.h)

set(LOGGER_SOURCES
        logging/NOOPLogger.cpp
        logging/NOOPLogger.h
        logging/ChronoLogger.cpp
        logging/ChronoLogger.h)

set(SDLOUTPUT_SOURCES
        sdl/SdlFrameOutput.cpp
        sdl/SdlFrameOutput.h)

if (UNIX)
    message("Detected Linux system")
    set(V4LOUTPUT_SOURCES v4l/V4LFrameOutput.cpp v4l/V4LFrameOutput.h)
    set(PULSE_SOURCES pulseaudio/PulseAudioOutput.cpp pulseaudio/PulseAudioOutput.h)

    set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${V4LOUTPUT_SOURCES} ${PULSE_SOURCES} ${SDLOUTPUT_SOURCES} ${LOGGER_SOURCES})
    set(PLATFORM_LIBS pthread pulse-simple)
else()
    message("Detected non-linux system")
    set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${SDLOUTPUT_SOURCES} ${LOGGER_SOURCES})
    set(PLATFORM_LIBS )
endif (UNIX)

add_library(liblgx STATIC ${LIBLGX_SOURCES})

target_link_libraries(liblgx PUBLIC usb-1.0 pthread SDL2_gfx SDL2 ${PLATFORM_LIBS})
target_include_directories(liblgx PUBLIC .)
target_include_directories(liblgx PUBLIC /usr/include/SDL2)
