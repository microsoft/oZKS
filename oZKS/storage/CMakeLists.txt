# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/memory_storage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/memory_storage_batch_inserter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/memory_storage_cache.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/batch_storage.h
        ${CMAKE_CURRENT_LIST_DIR}/memory_storage.h
        ${CMAKE_CURRENT_LIST_DIR}/memory_storage_batch_inserter.h
        ${CMAKE_CURRENT_LIST_DIR}/memory_storage_cache.h
        ${CMAKE_CURRENT_LIST_DIR}/memory_storage_helpers.h
        ${CMAKE_CURRENT_LIST_DIR}/storage.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/storage
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
