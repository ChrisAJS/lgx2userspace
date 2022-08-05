cmake_minimum_required(VERSION 3.18)
project(lgx2userspace)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(src/liblgx)
add_subdirectory(src/cli)
add_subdirectory(src/gtk)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_executable(usbsneef utils/usbsneef.c)

target_link_libraries(usbsneef PRIVATE pcap)

## Lovingly borrowed from https://www.mattkeeter.com/blog/2018-01-06-versioning/
execute_process(COMMAND git log --pretty=format:'%h' -n 1
        OUTPUT_VARIABLE GIT_REV
        ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else()
    execute_process(
            COMMAND bash -c "git diff --quiet --exit-code || echo +"
            OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
            COMMAND git describe --exact-match --tags
            OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE GIT_BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
endif()

set(VERSION "const char* GIT_REV=\"${GIT_REV}${GIT_DIFF}\";
const char* GIT_TAG=\"${GIT_TAG}\";
const char* GIT_BRANCH=\"${GIT_BRANCH}\";")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h "${VERSION}")
endif()
