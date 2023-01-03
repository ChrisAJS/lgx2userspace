cmake_minimum_required(VERSION 3.18)

enable_testing()

set(CMAKE_CXX_STANDARD 14)

set(V4LOUTPUT_SOURCES
        src/v4l/V4LFrameOutput.cpp
        src/v4l/V4LFrameOutput.h)

set(PULSE_SOURCES
        src/pulseaudio/PulseAudioOutput.cpp
        src/pulseaudio/PulseAudioOutput.h)

add_library(liblgx-linux STATIC
        ${V4LOUTPUT_SOURCES}
        ${PULSE_SOURCES})

target_include_directories(liblgx-linux PUBLIC src)

target_include_directories(liblgx-linux PRIVATE ../src)

# Although SDL is a 'common' dependency', each platform can resolve it
# in a different way. This will hopefully be pulled up into common once
# the performance difference between prebuilt linux libs and conan built
# libraries are understood
target_link_libraries(liblgx-linux PUBLIC usb-1.0 pthread SDL2 pulse-simple)

add_dependencies(liblgx-linux liblgx)
