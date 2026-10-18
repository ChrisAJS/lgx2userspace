cmake_minimum_required(VERSION 3.18)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

set(BOOTSTRAP_SOURCES
        src/bootstrap/commanddata_lgx.cpp
        src/bootstrap/commanddata_lgx.h
        src/bootstrap/commanddata_lgx2.cpp
        src/bootstrap/commanddata_lgx2.h)

set(USB_SOURCES
        src/usb/UsbStream.cpp
        src/usb/UsbStream.h)

add_library(liblgx-usb STATIC ${USB_SOURCES} ${BOOTSTRAP_SOURCES})

target_include_directories(liblgx-usb PUBLIC src)
target_link_libraries(liblgx-usb PUBLIC liblgx-core PkgConfig::LIBUSB)
