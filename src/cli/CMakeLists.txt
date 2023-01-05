cmake_minimum_required(VERSION 3.18)

set(SOURCES main.cpp OptionParser.cpp OptionParser.h)
set(GLFW_SOURCES glfwmain.cpp OptionParser.cpp OptionParser.h)

IF (CMAKE_SYSTEM MATCHES Windows)
    message("Building Windows executable")
    add_executable(lgx2userspace WIN32 ${SOURCES} appicon.rc)
    target_include_directories(lgx2userspace PRIVATE liblgx)
    target_link_libraries(lgx2userspace PRIVATE mingw32 liblgx -static)

    add_executable(lgx2userspace-glfw WIN32 ${GLFW_SOURCES} appicon.rc)
    target_include_directories(lgx2userspace-glfw PRIVATE liblgx)
    target_link_libraries(lgx2userspace-glfw PRIVATE mingw32 liblgx -static)
else()
    add_executable(lgx2userspace ${SOURCES})
    target_include_directories(lgx2userspace PRIVATE liblgx)
    target_link_libraries(lgx2userspace PRIVATE liblgx)

    add_executable(lgx2userspace-glfw ${GLFW_SOURCES})
    target_include_directories(lgx2userspace-glfw PRIVATE liblgx)
    target_link_libraries(lgx2userspace-glfw PRIVATE liblgx)
endif()