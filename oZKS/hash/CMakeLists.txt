# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/blake2b.c
    ${CMAKE_CURRENT_LIST_DIR}/blake2xb.c
    ${CMAKE_CURRENT_LIST_DIR}/hash.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/hash.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/hash
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
