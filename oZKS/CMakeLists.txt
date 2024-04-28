# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/commitment.cpp
    ${CMAKE_CURRENT_LIST_DIR}/compressed_trie.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ct_node.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ct_node_linked.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ct_node_stored.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ecpoint.cpp
    ${CMAKE_CURRENT_LIST_DIR}/insert_result.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ozks_config.cpp
    ${CMAKE_CURRENT_LIST_DIR}/partial_label.cpp
    ${CMAKE_CURRENT_LIST_DIR}/query_result.cpp
    ${CMAKE_CURRENT_LIST_DIR}/serialization_helpers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utilities.cpp
    ${CMAKE_CURRENT_LIST_DIR}/version.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vrf.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vrf_cache.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/commitment.h
        ${CMAKE_CURRENT_LIST_DIR}/compressed_trie.h
        ${CMAKE_CURRENT_LIST_DIR}/ct_node.h
        ${CMAKE_CURRENT_LIST_DIR}/ct_node_linked.h
        ${CMAKE_CURRENT_LIST_DIR}/ct_node_stored.h
        ${CMAKE_CURRENT_LIST_DIR}/defines.h
        ${CMAKE_CURRENT_LIST_DIR}/ecpoint.h
        ${CMAKE_CURRENT_LIST_DIR}/insert_result.h
        ${CMAKE_CURRENT_LIST_DIR}/ozks_config.h
        ${CMAKE_CURRENT_LIST_DIR}/partial_label.h
        ${CMAKE_CURRENT_LIST_DIR}/query_result.h
        ${CMAKE_CURRENT_LIST_DIR}/serialization_helpers.h
        ${CMAKE_CURRENT_LIST_DIR}/thread_pool.h
        ${CMAKE_CURRENT_LIST_DIR}/utilities.h
        ${CMAKE_CURRENT_LIST_DIR}/version.h
        ${CMAKE_CURRENT_LIST_DIR}/vrf.h
        ${CMAKE_CURRENT_LIST_DIR}/vrf_cache.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS
)

add_subdirectory(fourq)
add_subdirectory(hash)
add_subdirectory(providers)
add_subdirectory(storage)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
