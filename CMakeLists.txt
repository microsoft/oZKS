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

project(OZKS VERSION 0.3.0 LANGUAGES CXX C)

# This policy was introduced in CMake 3.13; OLD by default until CMake 3.21
cmake_policy(SET CMP0077 NEW)

########################
# Global configuration #
########################

# CMake modules
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(CheckLanguage)

# Extra modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# [option] OZKS_OPENENCLAVE (default: OFF)
# Compile OZKS library for Open Enclave
set(OZKS_OPENENCLAVE_OPTION_STR "Compile oZKS for Open Enclave")
option(OZKS_OPENENCLAVE ${OZKS_OPENENCLAVE_OPTION_STR} OFF)
message(STATUS "Compile oZKS for Open Enclave: ${OZKS_OPENENCLAVE}")

# Always build position-independent-code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Make the install target depend on the all target (required by vcpkg)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY OFF)

# Path for output
set(OUTLIB_PATH "lib")

# Required files and directories
include(GNUInstallDirs)
include(OZKSMacros)

# Source Tree
set(OZKS_INCLUDES_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OZKS_CONFIG_IN_FILENAME ${CMAKE_CURRENT_LIST_DIR}/cmake/OZKSConfig.cmake.in)
set(OZKS_CONFIG_H_IN_FILENAME ${CMAKE_CURRENT_LIST_DIR}/oZKS/config.h.in)

# Build tree
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${OUTLIB_PATH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${OUTLIB_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(OZKS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(OZKS_TARGETS_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/cmake/OZKSTargets.cmake)
set(OZKS_CONFIG_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/cmake/OZKSConfig.cmake)
set(OZKS_CONFIG_VERSION_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/cmake/OZKSConfigVersion.cmake)
set(OZKS_CONFIG_H_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/oZKS/config.h)

# Install
set(OZKS_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/OZKS-${OZKS_VERSION_MAJOR}.${OZKS_VERSION_MINOR})
set(OZKS_INCLUDES_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/OZKS-${OZKS_VERSION_MAJOR}.${OZKS_VERSION_MINOR})

# Use this if you want to compile with OpenEnclave SDK
if(OZKS_OPENENCLAVE)
    list(APPEND COMPILE_LIBCXX -stdlib=libc++)
    list(APPEND LINK_LIBCXX -lc++ -lc++abi -stdlib=libc++)
endif()

#########################
# External Dependencies #
#########################

# All dependencies are assumed pre-installed.
# find_package might throw a FATAL_ERROR before"xxx: not found", e.g. with vcpkg.cmake.

# Find Microsoft GSL
find_package(Microsoft.GSL 4 CONFIG QUIET REQUIRED)
if(NOT Microsoft.GSL_FOUND)
    message(FATAL_ERROR "Microsoft GSL: not found")
else()
    message(STATUS "Microsoft GSL: found")
endif()

# Flatbuffers
find_package(Flatbuffers REQUIRED)
if(NOT Flatbuffers_FOUND)
    message(FATAL_ERROR "Flatbuffers: not found")
else()
    message(STATUS "Flatbuffers: found")
    get_target_property(FLATBUFFERS_FLATC_PATH flatbuffers::flatc IMPORTED_LOCATION_RELEASE)
    include(CompileSchemaCXX)
endif()

# Poco
find_package(Poco CONFIG REQUIRED COMPONENTS Foundation)
if (NOT Poco_FOUND)
    message(FATAL_ERROR "Poco: not found")
else()
    message(STATUS "Poco: found")
endif()

# [Option] OZKS_BUILD_TESTS (default: ON)
set(OZKS_BUILD_TESTS_OPTION_STR "Build unit and integration tests for OZKS")
option(OZKS_BUILD_TESTS ${OZKS_BUILD_TESTS_OPTION_STR} ON)
if(OZKS_BUILD_TESTS)
    # Google Test
    find_package(GTest CONFIG REQUIRED)
    if(NOT GTest_FOUND)
        message(FATAL_ERROR "GTest: not found")
    else()
        message(STATUS "GTest: found")
    endif()
endif()

if(OZKS_OPENENCLAVE)
    # OpenEnclave
    find_package(OpenEnclave 0.17 CONFIG REQUIRED)
    set(OE_TARGET_ENCLAVE_AND_STD openenclave::oeenclave openenclave::oelibcxx openenclave::oelibc)
endif()

####################
# oZKS C++ library #
####################

# [option] BUILD_SHARED_LIBS (default: OFF)
# Build a shared library if set to ON. Build a static library regardlessly.
set(BUILD_SHARED_LIBS_STR "Build shared library")
option(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_STR} OFF)
# TODO: consider building shared in future
if(BUILD_SHARED_LIBS)
    message(FATAL_ERROR "Only static build is supported; set `BUILD_SHARED_LIBS=OFF`")
endif()

# Create the config file
configure_file(${OZKS_CONFIG_H_IN_FILENAME} ${OZKS_CONFIG_H_FILENAME})
install(FILES ${OZKS_CONFIG_H_FILENAME} DESTINATION ${OZKS_INCLUDES_INSTALL_DIR}/oZKS)

add_library(ozks STATIC)
ozks_set_version_filename(ozks)
ozks_set_language(ozks)
ozks_set_include_directories(ozks)
ozks_set_version(ozks)
ozks_install_target(ozks OZKSTargets)

target_link_libraries(ozks
    PUBLIC Microsoft.GSL::GSL
    PUBLIC flatbuffers::flatbuffers
    PUBLIC Poco::Foundation)

target_compile_options(ozks PUBLIC -DNOMINMAX)

# Ensure PDB is generated even in Release mode
if(MSVC)
    target_compile_options(ozks PUBLIC "/Zi")
endif()

if(OZKS_OPENENCLAVE)
    target_compile_options(ozks PRIVATE ${COMPILE_LIBCXX})
    target_link_options(ozks PRIVATE ${LINK_LIBCXX})
endif()

# Configurations for FourQlib: system, arch, SIMD, and assembler
target_compile_options(ozks PUBLIC -DHAVE_CONFIG)
target_compile_options(ozks PUBLIC -DUSE_SECURE_SEED)
target_compile_options(ozks PUBLIC -DUSE_ENDO=true)

# Set system
if(MSVC)
    target_compile_options(ozks PUBLIC -D__WINDOWS__)
elseif(UNIX)
    target_compile_options(ozks PUBLIC -D__LINUX__)
endif()

# Set architecture
include(DetectArch)
if(OZKS_FOURQ_AMD64)
    target_compile_options(ozks PUBLIC -D_AMD64_)
    message(STATUS "FourQlib optimization: arch=AMD64")
elseif(OZKS_FOURQ_ARM64)
    target_compile_options(ozks PUBLIC -D_ARM64_)
    message(STATUS "FourQlib optimization: arch=ARM64")
else()
    target_compile_options(ozks PUBLIC -D_GENERIC_)
    message(STATUS "FourQlib optimization: arch=GENERIC")
endif()
if(NOT MSVC)
    target_compile_options(ozks PUBLIC -march=native)
endif()

# Set AVX or AVX2 if not generic
if(OZKS_FOURQ_AMD64 OR OZKS_FOURQ_ARM64)
    include(FindAVX)
    check_for_avx(ozks)
    if(HAVE_AVX2_EXTENSIONS)
        target_compile_options(ozks PUBLIC -D_AVX2_)
        message(STATUS "FourQlib optimization: simd=AVX2")
    elseif(HAVE_AVX_EXTENSIONS)
        target_compile_options(ozks PUBLIC -D_AVX_)
        message(STATUS "FourQlib optimization: simd=AVX")
    endif()
else()
    message(STATUS "FourQlib optimization: simd=OFF")
endif()

# Use optimized assembly on Linux (non-AVX or AVX2)
set(OZKS_FOURQ_USE_ASM OFF)
if(UNIX AND NOT APPLE AND NOT CYGWIN AND NOT MINGW AND NOT OZKS_OPENENCLAVE)
    check_language(ASM)
    if(CMAKE_ASM_COMPILER)
        enable_language(ASM)
        target_compile_options(ozks PUBLIC -D_ASM_)
        set(OZKS_FOURQ_USE_ASM ON)
        message(STATUS "FourQlib optimization: asm=ON")
    endif()
endif()

# Add source files to library and header files to install
set(OZKS_SOURCE_FILES "")
add_subdirectory(oZKS)

target_sources(ozks PRIVATE ${OZKS_SOURCE_FILES})

####################################
# oZKS C++ library for OpenEnclave #
####################################

if(OZKS_OPENENCLAVE)
    add_library(ozks.enclave STATIC ${OZKS_SOURCE_FILES})

    ozks_set_version_filename(ozks.enclave)
    ozks_set_language(ozks.enclave)
    ozks_set_include_directories(ozks.enclave)
    ozks_set_version(ozks.enclave)
    ozks_install_target(ozks.enclave OZKSTargets)

    target_compile_options(ozks.enclave PUBLIC -nostdinc -nostdinc++)
    target_link_libraries(ozks.enclave PUBLIC ${OE_TARGET_ENCLAVE_AND_STD})

    target_link_libraries(ozks.enclave
        PUBLIC Microsoft.GSL::GSL
        PUBLIC flatbuffers::flatbuffers
        PUBLIC Poco::Foundation)

    # Configurations for FourQlib: system, arch, SIMD, and assembler
    target_compile_options(ozks.enclave PUBLIC -DHAVE_CONFIG)
    target_compile_options(ozks.enclave PUBLIC -DUSE_SECURE_SEED)
    target_compile_options(ozks.enclave PUBLIC -DUSE_ENDO=true)

    # Set system
    if(MSVC)
        target_compile_options(ozks.enclave PUBLIC -D__WINDOWS__)
    elseif(UNIX)
        target_compile_options(ozks.enclave PUBLIC -D__LINUX__)
    endif()

    # Set architecture
    target_compile_options(ozks.enclave PUBLIC -D_AMD64_)
    target_compile_options(ozks.enclave PUBLIC -D_GENERIC_)
    message(STATUS "FourQlib optimization: arch=AMD64, Generic implementation")

    # Add source files to library
    target_sources(ozks.enclave PRIVATE ${OZKS_SOURCE_FILES})
endif()

#################################
# Installation and CMake config #
#################################

# Create the CMake config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${OZKS_CONFIG_IN_FILENAME} ${OZKS_CONFIG_FILENAME}
    INSTALL_DESTINATION ${OZKS_CONFIG_INSTALL_DIR})

# Install the export
install(
    EXPORT OZKSTargets
    NAMESPACE OZKS::
    DESTINATION ${OZKS_CONFIG_INSTALL_DIR})

# Version file; we require exact version match for downstream
write_basic_package_version_file(
    ${OZKS_CONFIG_VERSION_FILENAME}
    VERSION ${OZKS_VERSION}
    COMPATIBILITY SameMinorVersion)

# Install config and module files
install(
    FILES
        ${OZKS_CONFIG_FILENAME}
        ${OZKS_CONFIG_VERSION_FILENAME}
    DESTINATION ${OZKS_CONFIG_INSTALL_DIR})

# We export OZKSTargets from the build tree so it can be used by other projects
# without requiring an install.
export(
    EXPORT OZKSTargets
    NAMESPACE OZKS::
    FILE ${OZKS_TARGETS_FILENAME})

###################
# oZKS unit tests #
###################

if(OZKS_BUILD_TESTS)
    add_executable(unit_tests)
    add_subdirectory(UnitTests)
    target_compile_options(unit_tests PRIVATE ${COMPILE_LIBCXX})
    target_link_libraries(unit_tests PRIVATE ozks GTest::gtest)
    target_link_options(unit_tests PRIVATE ${LINK_LIBCXX})
endif()
