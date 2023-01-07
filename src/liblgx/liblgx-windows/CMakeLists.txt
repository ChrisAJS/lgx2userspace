cmake_minimum_required(VERSION 3.18)

enable_testing()

add_library(liblgx-windows STATIC src/lgxplatform.h src/WindowsErrorSink.cpp src/WindowsErrorSink.h)

target_include_directories(liblgx-windows PUBLIC src)
target_include_directories(liblgx-windows PRIVATE ../src)

add_dependencies(liblgx-windows liblgx)