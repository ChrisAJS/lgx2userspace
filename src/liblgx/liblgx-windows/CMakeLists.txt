cmake_minimum_required(VERSION 3.18)

enable_testing()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_library(liblgx-windows STATIC src/lgxplatform.h src/unused.cpp)

target_include_directories(liblgx-windows PUBLIC src)

add_dependencies(liblgx-windows liblgx)