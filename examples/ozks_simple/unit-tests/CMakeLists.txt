# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(ozks-simple-unit-tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/unit_tests_runner.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ozks_tests.cpp
)
