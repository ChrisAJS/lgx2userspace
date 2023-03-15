list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR})

if (UNIX)
    set(CONAN_OS Linux)
endif ()

IF (CMAKE_SYSTEM MATCHES Windows)
    set(CONAN_OS Windows)
    set(ADDITIONAL_LIBS -static-libgcc -mwindows mingw32)
endif ()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES libusb/1.0.26 GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} os=${CONAN_OS})

find_package(libusb)
find_package(opengl_system)

set(PLATFORM_LIBS SDL2 libusb::libusb ${ADDITIONAL_LIBS})
set(PLATFORM_INCLUDES ${libusb_INCLUDE_DIRS} ${opengl_INCLUDE_DIRS})