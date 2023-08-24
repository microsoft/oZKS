# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(ozks-distributed-unit-tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/unit_tests_runner.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ozks_dist_tests.cpp
)
