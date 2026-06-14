cmake_minimum_required(VERSION 3.18)

find_library(AO_LIB ao REQUIRED)

set(V4LOUTPUT_SOURCES
        src/v4l/V4LFrameOutput.cpp
        src/v4l/V4LFrameOutput.h)

set(LIBAO_SOURCES
        src/libao/AoAudioOutput.cpp
        src/libao/AoAudioOutput.h)

add_library(liblgx-linux STATIC
        ${V4LOUTPUT_SOURCES}
        ${LIBAO_SOURCES})

target_include_directories(liblgx-linux PUBLIC src)
target_include_directories(liblgx-linux PRIVATE ../src)

target_link_libraries(liblgx-linux PUBLIC ${AO_LIB})
