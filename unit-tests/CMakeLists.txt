# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(unit_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/unit_tests_runner.cpp
        ${CMAKE_CURRENT_LIST_DIR}/commitment_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/compressed_trie_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/config_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ct_node_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ecpoint_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/insert_result_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/p256point_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/partial_label_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/query_result_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/utilities_tests.cpp
        ${CMAKE_CURRENT_LIST_DIR}/vrf_tests.cpp
)
