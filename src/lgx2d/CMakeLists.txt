cmake_minimum_required(VERSION 3.18)

# Capture daemon: hotplug, bootstrap and capture to a V4L2 loopback device. No SDL.
add_executable(lgx2d main.cpp CaptureDaemon.cpp CaptureDaemon.h)
target_link_libraries(lgx2d PRIVATE liblgx-core liblgx-usb liblgx-linux)
