cmake_minimum_required(VERSION 3.18)
project(lgx2userspace)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

option(USE_SFML "Use SFML over SDL" ON)

add_subdirectory(src/liblgx)
add_subdirectory(src/cli)

IF (CMAKE_SYSTEM MATCHES Darwin)
    message("Configuring for OSX")

    message("GTK+ frontend only supported on Linux")
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

    IF (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
    ENDIF()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_configure(REQUIRES libusb/1.0.26 sdl/2.0.20 sfml/2.5.1 GENERATORS cmake_find_package IMPORTS "bin, *.dll -> ./bin" IMPORTS "lib, *.dylib* -> ./bin")

    conan_cmake_autodetect(settings)

    conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} os=Macos)
ENDIF()

IF (CMAKE_SYSTEM MATCHES Linux)
    message("Configuring for Linux")
    add_subdirectory(src/gtk)
    add_executable(usbsneef utils/usbsneef.c)

    target_link_libraries(usbsneef PRIVATE pcap)
ENDIF()

IF (CMAKE_SYSTEM MATCHES Windows)
    message("Configuring for Windows")

    message("GTK+ frontend only supported on Linux")
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

    IF (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
    ENDIF()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_configure(REQUIRES libusb/1.0.26 sdl/2.0.20 sfml/2.5.1 GENERATORS cmake_find_package)

    conan_cmake_autodetect(settings)

    conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} os=Windows)

ENDIF()

include(version.cmake)
