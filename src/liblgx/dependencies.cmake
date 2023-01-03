if (UNIX)
    set(CONAN_OS Linux)
endif ()

IF (CMAKE_SYSTEM MATCHES Windows)
    set(CONAN_OS Windows)
    set(ADDITIONAL_LIBS -static-libgcc -mwindows mingw32)
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES libusb/1.0.26 sdl/2.26.1 GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${settings} os=${CONAN_OS})

find_package(libusb)
find_package(SDL2)

set(PLATFORM_LIBS SDL2::SDL2main SDL2::SDL2 libusb::libusb ${ADDITIONAL_LIBS})
set(PLATFORM_INCLUDES ${SDL2_INCLUDE_DIRS} ${libusb_INCLUDE_DIRS})