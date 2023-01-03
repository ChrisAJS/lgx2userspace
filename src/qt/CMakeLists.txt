cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 14)

set(Qt5_DIR /home/chris/Qt/Tools/QtDesignStudio/qt5_design_studio_reduced_version/lib/cmake/Qt5)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(lgx2userspace-qt
        src/main.cpp
        src/LgxWidget.cpp
        src/LgxWidget.h
        src/LgxDeviceThread.cpp
        src/LgxDeviceThread.h
        src/OpenGLDisplay.cpp
        src/OpenGLDisplay.h)

target_link_libraries(lgx2userspace-qt
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        liblgx
        )

target_include_directories(lgx2userspace-qt PRIVATE liblgx)
