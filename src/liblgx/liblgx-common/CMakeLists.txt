cmake_minimum_required(VERSION 3.18)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2    REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(LIBUSB  REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(GLFW3   REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(GLEW    REQUIRED IMPORTED_TARGET glew)
find_package(OpenGL REQUIRED)

set(BOOTSTRAP_SOURCES
        src/bootstrap/commanddata_lgx.cpp
        src/bootstrap/commanddata_lgx.h
        src/bootstrap/commanddata_lgx2.cpp
        src/bootstrap/commanddata_lgx2.h)

set(SDLOUTPUT_SOURCES
        src/sdl/SdlVideoOutput.cpp
        src/sdl/SdlVideoOutput.h
        src/sdl/SdlAudioOutput.cpp
        src/sdl/SdlAudioOutput.h)

set(GLFWOUTPUT_SOURCES
        src/glfw/GlfwVideoOutput.cpp
        src/glfw/GlfwVideoOutput.h)

set(NULL_SOURCES
        src/NullVideoOutput.cpp
        src/NullVideoOutput.h
        src/NullAudioOutput.cpp
        src/NullAudioOutput.h)

set(FAKE_USB_SOURCES
        src/FakeUsbStream.cpp
        src/FakeUsbStream.h)

set(USB_SOURCES
        src/usb/UsbStream.cpp
        src/usb/UsbStream.h)

set(ERRORSINK_SOURCES
        src/SimpleErrorSink.cpp
        src/SimpleErrorSink.h)

add_library(liblgx-common STATIC
        ${SDLOUTPUT_SOURCES}
        ${GLFWOUTPUT_SOURCES}
        ${NULL_SOURCES}
        ${USB_SOURCES}
        ${BOOTSTRAP_SOURCES}
        ${FAKE_USB_SOURCES}
        ${ERRORSINK_SOURCES}
        src/lgxcommon.h)

target_include_directories(liblgx-common PUBLIC src)
target_include_directories(liblgx-common PRIVATE ../src)

target_link_libraries(liblgx-common PUBLIC
        PkgConfig::SDL2
        PkgConfig::LIBUSB
        PkgConfig::GLFW3
        PkgConfig::GLEW
        OpenGL::GL)

if(ENABLE_LGX_GC550_SUPPORT)
    target_compile_definitions(liblgx-common PUBLIC GC550_SUPPORT)
endif()
