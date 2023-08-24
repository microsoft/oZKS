# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

if(OZKS_USE_INTRIN)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_QUIET TRUE)
    if(NOT MSVC)
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -O0 ${OZKS_LANG_FLAG}")
    endif()

    if(MSVC)
        # Check for _BitScanReverse64
        check_cxx_source_runs("
            #include <${OZKS_INTRIN_HEADER}>
            int main() {
                unsigned long a = 0, b = 0;
                volatile unsigned char res = _BitScanReverse64(&a, b);
                return 0;
            }"
            OZKS__BITSCANREVERSE64_FOUND
        )
    else()
        # Check for __builtin_clzll
        check_cxx_source_runs("
            int main() {
                volatile auto res = __builtin_clzll(0);
                return 0;
            }"
            OZKS___BUILTIN_CLZLL_FOUND
        )
    endif()

    cmake_pop_check_state()
endif()
