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

project(ozks_perf VERSION 0.3.0 LANGUAGES CXX)

# If not called from root CMakeLists.txt
if(NOT DEFINED OZKS_BUILD_BENCH)
    set(OZKS_BUILD_BENCH ON)

    # Import OZKS
    find_package(OZKS 0.3.0 EXACT REQUIRED)

    # Must define these variables and include macros
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${OUTLIB_PATH})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif()

# if OZKS_BUILD_BENCH is ON, use GoogleBenchmark
if(OZKS_BUILD_BENCH)
    find_package(benchmark REQUIRED)
    if(NOT benchmark_FOUND)
        message(FATAL_ERROR "GoogleBenchmark: not found")
    else()
        message(STATUS "GoogleBenchmark: found")
    endif()

    find_package(Microsoft.GSL 4 CONFIG QUIET REQUIRED)
    if(NOT Microsoft.GSL_FOUND)
        message(FATAL_ERROR "Microsoft GSL: not found")
    else()
        message(STATUS "Microsoft GSL: found")
    endif()

    find_package(Flatbuffers REQUIRED)
    if(NOT Flatbuffers_FOUND)
        message(FATAL_ERROR "Flatbuffers: not found")
    else()
        message(STATUS "Flatbuffers: found")
    endif()

    add_executable(ozks_perf)
    target_sources(ozks_perf
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/mem_utils.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ozks_perf_tests.cpp
    )

    # Ensure PDB is generated even in Release mode
    if(MSVC)
        target_compile_options(ozks_perf PUBLIC "/Zi")
    endif()

    if(TARGET OZKS::ozks)
        target_link_libraries(ozks_perf PUBLIC OZKS::ozks benchmark::benchmark)
    else()
        message(FATAL_ERROR "Cannot find target OZKS::ozks")
    endif()
endif()
