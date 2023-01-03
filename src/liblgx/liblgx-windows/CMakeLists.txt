cmake_minimum_required(VERSION 3.18)

enable_testing()

add_library(liblgx-windows STATIC src/lgxplatform.h src/unused.cpp)

target_include_directories(liblgx-windows PUBLIC src)

add_dependencies(liblgx-windows liblgx)