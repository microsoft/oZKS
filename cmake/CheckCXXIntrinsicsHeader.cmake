# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Check for intrin.h or x86intrin.h
if(OZKS_USE_INTRIN)
    set(CMAKE_REQUIRED_QUIET_OLD ${CMAKE_REQUIRED_QUIET})
    set(CMAKE_REQUIRED_QUIET ON)

    if(MSVC)
        set(OZKS_INTRIN_HEADER "intrin.h")
    else()
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(OZKS_ARM64 ON)
        else()
            set(OZKS_ARM64 OFF)
        endif()
        if(OZKS_ARM64)
            set(OZKS_INTRIN_HEADER "arm_neon.h")
        elseif(EMSCRIPTEN)
            set(OZKS_INTRIN_HEADER "wasm_simd128.h")
        else()
            set(OZKS_INTRIN_HEADER "x86intrin.h")
        endif()
    endif()

    check_include_file_cxx(${OZKS_INTRIN_HEADER} OZKS_INTRIN_HEADER_FOUND)
    set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_OLD})

    if(OZKS_INTRIN_HEADER_FOUND)
        message(STATUS "Intrinsics header found (${OZKS_INTRIN_HEADER})")
    else()
        message(STATUS "Intrinsics header not found")
    endif()
endif()
