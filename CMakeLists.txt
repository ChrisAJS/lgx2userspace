cmake_minimum_required(VERSION 3.18)
project(lgx2userspace)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_subdirectory(src/liblgx)
add_subdirectory(src/cli)

include(conan.cmake)

set(BASE_PACKAGES libusb/1.0.26 sdl/2.0.20 libpcap/1.10.1)

IF (CMAKE_SYSTEM MATCHES Windows)
    message("GTK+ frontend only supported on Linux")

    set(PLATFORM_SPECIFIC_PACKAGES )

    set(PLATFORM_SPECIFIC_CONAN_CONFIG os=Windows)
else(CMAKE_SYSTEM MATCHES Windows)
    add_subdirectory(src/gtk)

    set(PLATFORM_SPECIFIC_PACKAGES pulseaudio/14.2)
endif(CMAKE_SYSTEM MATCHES Windows)

conan_cmake_configure(REQUIRES ${BASE_PACKAGES} ${PLATFORM_SPECIFIC_PACKAGES} GENERATORS cmake_find_package)
conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} ${PLATFORM_SPECIFIC_CONAN_CONFIG})

find_package(libpcap)

add_executable(usbsneef utils/usbsneef.c)

target_link_libraries(usbsneef PRIVATE libpcap::libpcap)
target_include_directories(usbsneef PRIVATE libpcap::libpcap)

include(version.cmake)
