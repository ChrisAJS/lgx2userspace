
if (UNIX)
    set(PLATFORM_LIBS pthread SDL2)
    set(PLATFORM_INCLUDES)
endif ()

IF (CMAKE_SYSTEM MATCHES Windows)

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

    find_package(libusb)
    find_package(SDL2)

    set(PLATFORM_LIBS mingw32 SDL2::SDL2main SDL2::SDL2 libusb::libusb -static-libgcc -mwindows)
    set(PLATFORM_INCLUDES ${SDL2_INCLUDE_DIRS} ${libusb_INCLUDE_DIRS})

endif ()