cmake_minimum_required(VERSION 3.18)

enable_testing()

add_subdirectory(liblgx-common)

set(LIBLGX_BASE_SOURCES
        src/liblgx.h
        src/lgxdevice.cpp
        src/lgxdevice.h
        src/FrameBuilder.cpp
        src/FrameBuilder.h)

set(LOGGER_SOURCES
        src/logging/NOOPLogger.cpp
        src/logging/NOOPLogger.h
        src/logging/ChronoLogger.cpp
        src/logging/ChronoLogger.h)

set(LIBLGX_SOURCES ${LIBLGX_BASE_SOURCES} ${LOGGER_SOURCES})

add_library(liblgx STATIC ${LIBLGX_SOURCES})

# Platform specific configurations
if (CMAKE_SYSTEM MATCHES Linux)
    message("Detected Linux system")
    add_subdirectory(liblgx-linux)
    target_link_libraries(liblgx PUBLIC liblgx-linux)
endif()

if (CMAKE_SYSTEM MATCHES Darwin)
    message("Detected Mac OS system")
    add_subdirectory(liblgx-macos)
    target_link_libraries(liblgx PUBLIC liblgx-macos)
endif()

IF (CMAKE_SYSTEM MATCHES Windows)
    message("Detected Windows system")
    add_subdirectory(liblgx-windows)
    target_link_libraries(liblgx PUBLIC liblgx-windows)
endif()

target_link_libraries(liblgx PUBLIC liblgx-common)
target_include_directories(liblgx PUBLIC src)

# Tests
add_executable(framebuilder_test test/FrameBuilder.t.cpp test/catch_amalgamated.cpp test/catch_amalgamated.hpp)
target_include_directories(framebuilder_test PRIVATE src)
target_link_libraries(framebuilder_test PRIVATE liblgx)
add_test(framebuilder_test framebuilder_test)
