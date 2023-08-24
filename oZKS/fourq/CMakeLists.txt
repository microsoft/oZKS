# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

if(OZKS_USE_OPENSSL_P256)
# Don't actually build FourQ in this case; we just need the
# random_bytes function

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/random.c
)

else()
# Build FourQlib for use in VRF implementation

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/crypto_util.c
    ${CMAKE_CURRENT_LIST_DIR}/eccp2_core.c
    ${CMAKE_CURRENT_LIST_DIR}/eccp2_no_endo.c
    ${CMAKE_CURRENT_LIST_DIR}/eccp2.c
    ${CMAKE_CURRENT_LIST_DIR}/hash_to_curve.c
    ${CMAKE_CURRENT_LIST_DIR}/random.c
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/FourQ_api.h
        ${CMAKE_CURRENT_LIST_DIR}/FourQ_internal.h
        ${CMAKE_CURRENT_LIST_DIR}/FourQ_params.h
        ${CMAKE_CURRENT_LIST_DIR}/FourQ_tables.h
        ${CMAKE_CURRENT_LIST_DIR}/FourQ.h
        ${CMAKE_CURRENT_LIST_DIR}/random.h
        ${CMAKE_CURRENT_LIST_DIR}/table_lookup.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/fourq
)

if(OZKS_ARCH_AMD64)
    add_subdirectory(amd64)
elseif(OZKS_ARCH_ARM64)
    add_subdirectory(arm64)
else()
    add_subdirectory(generic)
endif()

endif() # OZKS_USE_OPENSSL_P256

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
