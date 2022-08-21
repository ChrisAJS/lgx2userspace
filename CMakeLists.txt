cmake_minimum_required(VERSION 3.18)
project(lgx2userspace)

OPTION(ENABLE_LGX_GC550_SUPPORT "Enables LGX (GC550) support which will prevent further use of the LGX with the official Windows driver" OFF)

if (ENABLE_LGX_GC550_SUPPORT)
    message("You have opted to build in LGX (GC550) support into the userspace driver.")
    message("It is important to understand that *currently* it is not possible to use this")
    message("userspace driver and then go on to use the official manufacturer driver on Windows.")
    message("")
    message("Using this userspace driver with the LGX (GC550) will force you to only be able to use")
    message("this software in the future. Please consult the README.md for more information.")
    execute_process(COMMAND sleep 10)
else()
    message("Dangerous LGX (GC550) support will not be compiled into the lgx2userspace executable.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_subdirectory(src/liblgx)
add_subdirectory(src/cli)

IF (NOT CMAKE_SYSTEM MATCHES Windows)
    add_subdirectory(src/gtk)
    add_executable(usbsneef utils/usbsneef.c)

    target_link_libraries(usbsneef PRIVATE pcap)
else(NOT CMAKE_SYSTEM MATCHES Windows)
    message("GTK+ frontend only supported on Linux")
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_configure(REQUIRES libusb/1.0.26 sdl/2.0.20 GENERATORS cmake_find_package)

    conan_cmake_autodetect(settings)

    conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} os=Windows)

endif(NOT CMAKE_SYSTEM MATCHES Windows)

include(version.cmake)
