cmake_minimum_required(VERSION 3.18)


add_library(liblgx-windows STATIC src/lgxplatform.h src/WindowsErrorSink.cpp src/WindowsErrorSink.h)

target_include_directories(liblgx-windows PUBLIC src)
target_link_libraries(liblgx-windows PUBLIC liblgx-core)