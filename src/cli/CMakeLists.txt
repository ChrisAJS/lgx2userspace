cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_executable(lgx2userspace main.cpp OptionParser.cpp OptionParser.h)

target_include_directories(lgx2userspace PRIVATE /usr/include/SDL2)
target_include_directories(lgx2userspace PRIVATE liblgx)
target_link_libraries(lgx2userspace PRIVATE liblgx)