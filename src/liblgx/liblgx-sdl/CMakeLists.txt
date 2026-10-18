cmake_minimum_required(VERSION 3.18)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

set(SDLOUTPUT_SOURCES
        src/sdl/SdlVideoOutput.cpp
        src/sdl/SdlVideoOutput.h
        src/sdl/SdlAudioOutput.cpp
        src/sdl/SdlAudioOutput.h)

add_library(liblgx-sdl STATIC ${SDLOUTPUT_SOURCES})

target_include_directories(liblgx-sdl PUBLIC src)
target_link_libraries(liblgx-sdl PUBLIC liblgx-core PkgConfig::SDL3)
