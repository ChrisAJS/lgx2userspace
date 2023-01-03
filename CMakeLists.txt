cmake_minimum_required(VERSION 3.18)
project(lgx2userspace)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")

OPTION(ENABLE_LGX_GC550_SUPPORT "Enables LGX (GC550) support which will prevent further use of the LGX with the official Windows driver" OFF)

if (ENABLE_LGX_GC550_SUPPORT)
    message("You have opted to build in LGX (GC550) support into the userspace driver.")
    message("It is important to understand that *currently* it is not possible to use this")
    message("userspace driver and then go on to use the official manufacturer driver on Windows.")
    message("")
    message("Using this userspace driver with the LGX (GC550) will force you to only be able to use")
    message("this software in the future. Please consult the README.md for more information.")
    execute_process(COMMAND sleep 10)
else()
    message("Dangerous LGX (GC550) support will not be compiled into the lgx2userspace executable.")
endif()

add_subdirectory(src/liblgx)
add_subdirectory(src/cli)

include(version.cmake)
