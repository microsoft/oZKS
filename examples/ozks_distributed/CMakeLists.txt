# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(OZKSDISTRIBUTED_SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR})

include(CompileOZKSDistSchemaCXX)

add_library(ozks-distributed STATIC)
set(OZKSDISTRIBUTED_SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/ozks_distributed.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ozks_config_dist.cpp"
)

add_subdirectory("providers")

target_sources(ozks-distributed PRIVATE 
    ${OZKSDISTRIBUTED_SOURCE_FILES}
)

# Ensure PDB is generated even in Release mode
if(MSVC)
    target_compile_options(ozks-distributed PUBLIC "/Zi")
endif()

if(TARGET OZKS::ozks)
    target_link_libraries(ozks-distributed PUBLIC OZKS::ozks)
elseif(TARGET ozks)
    target_link_libraries(ozks-distributed PUBLIC ozks)
else()
    message(FATAL_ERROR "Cannot find target OZKS::ozks")
endif()


if(OZKS_BUILD_TESTS)
    add_executable(ozks-distributed-unit-tests)
    add_subdirectory(unit-tests)
    target_link_libraries(ozks-distributed-unit-tests PRIVATE ozks-distributed GTest::gtest)
endif()
