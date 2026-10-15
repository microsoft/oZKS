# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

cmake_minimum_required(VERSION 3.13)

# [option] CMAKE_BUILD_TYPE (default: "Release")
# Build in one of the following modes: Release, Debug, MiniSizeRel, or RelWithDebInfo.
# Most generators recognize these and can set the compiler flags accordingly. We set
# the build type here before creating the project to prevent the CMake generator from
# overriding our default of "Release".
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif()
message(STATUS "Build type (CMAKE_BUILD_TYPE): ${CMAKE_BUILD_TYPE}")

project(ozks-examples VERSION 2.0.0 LANGUAGES CXX)

# Standalone build: this directory is the top-level project rather than a subdirectory of the
# root CMakeLists.txt.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(OZKS_BUILD_EXAMPLES ON)

    # Standalone build: import an installed OZKS and define the variables the root build
    # would otherwise provide.
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/OZKSStandaloneSetup.cmake)
    ozks_standalone_setup()
endif()

if(OZKS_BUILD_EXAMPLES)
    add_subdirectory(ozks_simple)
    add_subdirectory(ozks_distributed)
endif()
