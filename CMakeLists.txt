# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)

# Pull in the app version from version.txt when present so releases are traceable.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/version.txt")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" PROJECT_VER)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-secure-ota)
