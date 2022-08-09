cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(SOURCES main.cpp OptionParser.cpp OptionParser.h)

IF (CMAKE_SYSTEM MATCHES Windows)
    message("Building Windows executable")
    add_executable(lgx2userspace WIN32 ${SOURCES} appicon.rc)
    target_link_libraries(lgx2userspace PRIVATE mingw32 liblgx)
else()
    add_executable(lgx2userspace ${SOURCES})
    target_include_directories(lgx2userspace PRIVATE liblgx)
    target_link_libraries(lgx2userspace PRIVATE liblgx)
endif()