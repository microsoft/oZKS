# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKSSIMPLE_SOURCE_FILES ${OZKSSIMPLE_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/local_query_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/local_trie_info_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/local_update_provider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mapped_tries.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/local_query_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/local_trie_info_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/local_update_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/mapped_tries.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/providers
)

set(OZKSSIMPLE_SOURCE_FILES ${OZKSSIMPLE_SOURCE_FILES} PARENT_SCOPE)

