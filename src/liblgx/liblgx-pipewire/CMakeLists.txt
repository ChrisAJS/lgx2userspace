cmake_minimum_required(VERSION 3.18)

# Only built when the parent found libpipewire-0.3 (PkgConfig::PIPEWIRE).
add_library(liblgx-pipewire STATIC
        src/pw/PipeWireAudioOutput.cpp
        src/pw/PipeWireAudioOutput.h
        src/pw/PipeWireAudioSource.cpp
        src/pw/PipeWireAudioSource.h)

target_include_directories(liblgx-pipewire PUBLIC src)
target_link_libraries(liblgx-pipewire PUBLIC liblgx-core PkgConfig::PIPEWIRE)
