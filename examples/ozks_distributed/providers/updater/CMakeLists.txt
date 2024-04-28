# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(OZKSDISTRIBUTED_SOURCE_FILES ${OZKSDISTRIBUTED_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/updater.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/updater.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/providers/updater
)

set(OZKSDISTRIBUTED_SOURCE_FILES ${OZKSDISTRIBUTED_SOURCE_FILES} PARENT_SCOPE)
