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

project(ozks-perf VERSION 2.0.0 LANGUAGES CXX)

# Standalone build: this directory is the top-level project rather than a subdirectory of the
# root CMakeLists.txt.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Import an installed OZKS and build the ozks-simple example library from its sibling
    # directory. ozks-simple is never installed as a package of its own, so it cannot be found
    # with find_package.
    include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/OZKSStandaloneSetup.cmake)
    ozks_standalone_setup()

    # ozks_simple adds this directory itself when OZKS_BUILD_BENCH is ON, so keep it OFF (and
    # tests OFF) while adding it, then build the benchmark below.
    set(OZKS_BUILD_TESTS OFF)
    set(OZKS_BUILD_BENCH OFF)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/ozks-simple)
    set(OZKS_BUILD_BENCH ON)
endif()

# if OZKS_BUILD_BENCH is ON, use GoogleBenchmark
if(OZKS_BUILD_BENCH)
    find_package(benchmark REQUIRED)
    if(NOT benchmark_FOUND)
        message(FATAL_ERROR "GoogleBenchmark: not found")
    else()
        message(STATUS "GoogleBenchmark: found")
    endif()

    add_executable(ozks-perf)
    target_sources(ozks-perf
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/mem_utils.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ozks_perf_tests.cpp
    )

    add_compile_definitions(BENCHMARK_STATIC_DEFINE)

    # Ensure PDB is generated even in Release mode
    if(MSVC)
        target_compile_options(ozks-perf PUBLIC "/Zi")
    endif()

    if(TARGET ozks-simple)
        target_link_libraries(ozks-perf PUBLIC ozks-simple benchmark::benchmark)
    else()
        message(FATAL_ERROR "Cannot find target ozks-simple")
    endif()
endif()
