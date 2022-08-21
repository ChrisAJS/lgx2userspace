cmake_minimum_required(VERSION 3.18)

enable_testing()

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

IF (ENABLE_LGX_GC550_SUPPORT)
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h "#define GC550_SUPPORT")
ELSE()
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h "#define GC550_NOT_SUPPORTED")
ENDIF()

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
    set(PLATFORM_LIBS usb-1.0 pthread SDL2 pthread pulse-simple)
    set(PLATFORM_INCLUDES /usr/include)
else()
    message("Detected non-linux system")
    IF (CMAKE_SYSTEM MATCHES Windows)
        message("Building liblgx for Windows")
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
        list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
        find_package(libusb)
        find_package(SDL2)

        set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${SDLOUTPUT_SOURCES} ${LOGGER_SOURCES})
        set(PLATFORM_LIBS mingw32 SDL2::SDL2main SDL2::SDL2 libusb::libusb -static-libgcc -mwindows)
        set(PLATFORM_INCLUDES ${SDL2_INCLUDE_DIRS} ${libusb_INCLUDE_DIRS})

        message("PLATFORM LIBS ${PLATFORM_LIBS}")
    endif()
endif (UNIX)

add_library(liblgx STATIC ${LIBLGX_SOURCES})

target_link_libraries(liblgx PUBLIC ${PLATFORM_LIBS})
target_include_directories(liblgx PUBLIC src)
target_include_directories(liblgx PUBLIC ${PLATFORM_INCLUDES})

add_executable(framebuilder_test test/FrameBuilder.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_include_directories(framebuilder_test PRIVATE src)
target_link_libraries(framebuilder_test PRIVATE liblgx)
add_test(framebuilder_test framebuilder_test)
