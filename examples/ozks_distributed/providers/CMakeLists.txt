# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKSDISTRIBUTED_SOURCE_FILES ${OZKSDISTRIBUTED_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/common_storage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dist_query_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dist_trie_info_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dist_update_provider.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/common_storage.h
        ${CMAKE_CURRENT_LIST_DIR}/dist_query_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/dist_trie_info_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/dist_update_provider.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/providers
)

add_subdirectory(querier)
add_subdirectory(updater)

set(OZKSDISTRIBUTED_SOURCE_FILES ${OZKSDISTRIBUTED_SOURCE_FILES} PARENT_SCOPE)

