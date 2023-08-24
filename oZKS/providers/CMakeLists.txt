# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/query_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/trie_info_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/update_provider.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/query_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/trie_info_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/update_provider.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/providers
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
