cmake_minimum_required(VERSION 3.18)

enable_testing()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

IF (ENABLE_LGX_GC550_SUPPORT)
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h "/* This file is generated as part of the CMake project configuration - See README.md for more details. */\n#define GC550_SUPPORT")
ELSE()
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h "/* This file is generated as part of the CMake project configuration - See README.md for more details. */\n#define GC550_NOT_SUPPORTED")
ENDIF()

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
if (UNIX)
    message("Detected Linux system")
    add_subdirectory(liblgx-linux)
    target_link_libraries(liblgx PUBLIC liblgx-linux)
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
