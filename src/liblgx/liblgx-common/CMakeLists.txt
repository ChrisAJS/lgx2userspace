cmake_minimum_required(VERSION 3.18)

enable_testing()

include(../../../dependencies.cmake)

set(CMAKE_CXX_STANDARD 14)

set(BOOTSTRAP_SOURCES
        src/bootstrap/commanddata_lgx.cpp
        src/bootstrap/commanddata_lgx.h
        src/bootstrap/commanddata_lgx2.cpp
        src/bootstrap/commanddata_lgx2.h)

set(SDLOUTPUT_SOURCES
        src/sdl/SdlVideoOutput.cpp
        src/sdl/SdlVideoOutput.h
        src/sdl/SdlAudioOutput.cpp
        src/sdl/SdlAudioOutput.h
        )

#set(GLFWOUTPUT_SOURCES
#        src/glfw/GlfwVideoOutput.cpp
#        src/glfw/GlfwVideoOutput.h)

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
target_include_directories(liblgx-common PUBLIC ${PLATFORM_INCLUDES})
target_include_directories(liblgx-common PRIVATE ../src)

target_link_libraries(liblgx-common PUBLIC ${PLATFORM_LIBS})

add_dependencies(liblgx-common liblgx)

