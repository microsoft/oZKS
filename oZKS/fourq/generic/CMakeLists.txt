# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/fp.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/fourq/generic
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
