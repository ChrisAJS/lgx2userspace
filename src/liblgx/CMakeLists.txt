cmake_minimum_required(VERSION 3.18)

enable_testing()

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

set(LIBLGX_BASE_SOURCES
        src/liblgx.h
        src/lgxdevice.cpp
        src/lgxdevice.h
        src/bootstrap/commanddata_lgx2.cpp
        src/bootstrap/commanddata_lgx2.h
        src/bootstrap/commanddata_lgx.cpp
        src/bootstrap/commanddata_lgx.h
        src/usb/UsbStream.cpp
        src/usb/UsbStream.h
        src/FrameBuilder.cpp
        src/FrameBuilder.h
        src/NullVideoOutput.cpp
        src/NullVideoOutput.h
        src/NullAudioOutput.cpp
        src/NullAudioOutput.h)

set(LOGGER_SOURCES
        src/logging/NOOPLogger.cpp
        src/logging/NOOPLogger.h
        src/logging/ChronoLogger.cpp
        src/logging/ChronoLogger.h)

set(SDLOUTPUT_SOURCES
        src/sdl/SdlFrameOutput.cpp
        src/sdl/SdlFrameOutput.h)

if (UNIX)
    message("Detected Linux system")
    set(V4LOUTPUT_SOURCES src/v4l/V4LFrameOutput.cpp src/v4l/V4LFrameOutput.h)
    set(PULSE_SOURCES src/pulseaudio/PulseAudioOutput.cpp src/pulseaudio/PulseAudioOutput.h)

    set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${V4LOUTPUT_SOURCES} ${PULSE_SOURCES} ${SDLOUTPUT_SOURCES} ${LOGGER_SOURCES})
    set(PLATFORM_LIBS pthread pulse-simple)
else()
    message("Detected non-linux system")
    set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${SDLOUTPUT_SOURCES} ${LOGGER_SOURCES})
    set(PLATFORM_LIBS )
endif (UNIX)

add_library(liblgx STATIC ${LIBLGX_SOURCES})

target_link_libraries(liblgx PUBLIC usb-1.0 pthread SDL2_gfx SDL2 ${PLATFORM_LIBS})
target_include_directories(liblgx PUBLIC src)
target_include_directories(liblgx PUBLIC /usr/include/SDL2)

add_executable(framebuilder_test test/FrameBuilder.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_include_directories(framebuilder_test PRIVATE src)
target_link_libraries(framebuilder_test PRIVATE liblgx)
add_test(framebuilder_test framebuilder_test)
