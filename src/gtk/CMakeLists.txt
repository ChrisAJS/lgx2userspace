cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtkmm-3.0)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -O3")

add_executable(lgx2userspace-gtk gtkmain.cpp)

target_include_directories(lgx2userspace-gtk PRIVATE ${GTK_INCLUDE_DIRS} liblgx)
message("GTKMM INCLUDE DIR: ${GTK_LIBRARIES}")

target_link_libraries(lgx2userspace-gtk PRIVATE ${GTK_LIBRARIES} liblgx)

target_link_directories(lgx2userspace-gtk PRIVATE ${GTK_LIBRARY_DIRS} liblgx)
