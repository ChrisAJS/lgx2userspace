cmake_minimum_required(VERSION 3.18)

enable_testing()

add_library(liblgx-macos STATIC src/lgxplatform.h src/MacOsErrorSink.cpp src/MacOsErrorSink.h)

target_include_directories(liblgx-macos PUBLIC src)
target_include_directories(liblgx-macos PRIVATE ../src)

add_dependencies(liblgx-macos liblgx)