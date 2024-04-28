# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(OZKSSIMPLE_SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR})

include(CompileOZKSSchemaCXX)

add_library(ozks-simple STATIC)
set(OZKSSIMPLE_SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/ozks.cpp")

add_subdirectory("providers")

target_sources(ozks-simple PRIVATE
    ${OZKSSIMPLE_SOURCE_FILES}
)

# Ensure PDB is generated even in Release mode
if(MSVC)
    target_compile_options(ozks-simple PUBLIC "/Zi")
endif()

if(TARGET OZKS::ozks)
    target_link_libraries(ozks-simple PUBLIC OZKS::ozks)
elseif(TARGET ozks)
    target_link_libraries(ozks-simple PUBLIC ozks)
else()
    message(FATAL_ERROR "Cannot find target OZKS::ozks")
endif()


if(OZKS_BUILD_TESTS)
    add_executable(ozks-simple-unit-tests)
    add_subdirectory(unit-tests)
    target_link_libraries(ozks-simple-unit-tests PRIVATE ozks-simple GTest::gtest)
endif()

if(OZKS_BUILD_BENCH)
    add_subdirectory(perf)
endif()
