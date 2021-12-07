# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
if(OZKS_FOURQ_USE_ASM)
    if(HAVE_AVX2_EXTENSIONS)
        set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
            ${CMAKE_CURRENT_LIST_DIR}/consts.c
            ${CMAKE_CURRENT_LIST_DIR}/fp2_1271_AVX2.S
        )
    else()
        set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
            ${CMAKE_CURRENT_LIST_DIR}/fp2_1271.S
        )
    endif()
endif()

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/fp_x64.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/fourq/amd64
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
