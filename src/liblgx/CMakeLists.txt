cmake_minimum_required(VERSION 3.18)

# Library layout (dependencies point downward only):
#   liblgx-core   protocol parsing, pipeline, interfaces, pure helpers - no external dependencies
#   liblgx-usb    libusb device access and bootstrap (links libusb)
#   liblgx-sdl    SDL window and audio outputs (links SDL3)
#   liblgx-linux / -macos / -windows   platform outputs and error sinks
#   liblgx        umbrella target linking all of the above, for the CLI

set(LIBLGX_CORE_SOURCES
        src/liblgx.h
        src/lgxinterfaces.h
        src/lgxdevice.cpp
        src/lgxdevice.h
        src/CapturePipeline.cpp
        src/CapturePipeline.h
        src/NoSignalCard.cpp
        src/NoSignalCard.h
        src/FrameBuilder.cpp
        src/FrameBuilder.h
        src/BufferReorderer.cpp
        src/BufferReorderer.h
        src/AudioGate.cpp
        src/AudioGate.h
        src/SignalStatus.cpp
        src/SignalStatus.h
        src/NullVideoOutput.cpp
        src/NullVideoOutput.h
        src/NullAudioOutput.cpp
        src/NullAudioOutput.h
        src/FakeUsbStream.cpp
        src/FakeUsbStream.h
        src/SimpleErrorSink.cpp
        src/SimpleErrorSink.h)

set(LOGGER_SOURCES
        src/logging/NOOPLogger.cpp
        src/logging/NOOPLogger.h
        src/logging/ChronoLogger.cpp
        src/logging/ChronoLogger.h)

add_library(liblgx-core STATIC ${LIBLGX_CORE_SOURCES} ${LOGGER_SOURCES})
target_include_directories(liblgx-core PUBLIC src)

# DeviceType (in lgxdevice.h) depends on this, so every library must see the same value.
if(ENABLE_LGX_GC550_SUPPORT)
    target_compile_definitions(liblgx-core PUBLIC GC550_SUPPORT)
endif()

add_subdirectory(liblgx-usb)
add_subdirectory(liblgx-sdl)

add_library(liblgx INTERFACE)
target_link_libraries(liblgx INTERFACE liblgx-core liblgx-usb liblgx-sdl)

# Platform specific configurations
if (CMAKE_SYSTEM MATCHES Linux)
    message("Detected Linux system")
    add_subdirectory(liblgx-linux)
    target_link_libraries(liblgx INTERFACE liblgx-linux)

    # Optional: PipeWire audio source for lgx2d.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    if (PIPEWIRE_FOUND)
        add_subdirectory(liblgx-pipewire)
    else()
        message("libpipewire-0.3 not found: lgx2d will be built without audio output")
    endif()
endif()

if (CMAKE_SYSTEM MATCHES Darwin)
    message("Detected Mac OS system")
    add_subdirectory(liblgx-macos)
    target_link_libraries(liblgx INTERFACE liblgx-macos)
endif()

IF (CMAKE_SYSTEM MATCHES Windows)
    message("Detected Windows system")
    add_subdirectory(liblgx-windows)
    target_link_libraries(liblgx INTERFACE liblgx-windows)
endif()

# Tests link only liblgx-core, which keeps it free of SDL and libusb.
add_executable(framebuilder_test test/FrameBuilder.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_link_libraries(framebuilder_test PRIVATE liblgx-core)
add_test(framebuilder_test framebuilder_test)

add_executable(signalstatus_test test/SignalStatus.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_link_libraries(signalstatus_test PRIVATE liblgx-core)
add_test(signalstatus_test signalstatus_test)

add_executable(bufferreorderer_test test/BufferReorderer.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_link_libraries(bufferreorderer_test PRIVATE liblgx-core)
add_test(bufferreorderer_test bufferreorderer_test)

add_executable(audiogate_test test/AudioGate.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_link_libraries(audiogate_test PRIVATE liblgx-core)
add_test(audiogate_test audiogate_test)

add_executable(capturepipeline_test test/CapturePipeline.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_link_libraries(capturepipeline_test PRIVATE liblgx-core)
add_test(capturepipeline_test capturepipeline_test)
