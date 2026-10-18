cmake_minimum_required(VERSION 3.18)


add_library(liblgx-macos STATIC src/lgxplatform.h src/MacOsErrorSink.cpp src/MacOsErrorSink.h)

target_include_directories(liblgx-macos PUBLIC src)
target_link_libraries(liblgx-macos PUBLIC liblgx-core)