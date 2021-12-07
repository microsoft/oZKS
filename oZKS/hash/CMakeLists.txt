

# Source files in this directory
set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/sha512.c
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/sha512.h
    DESTINATION
        ${OZKS_INCLUDES_INSTALL_DIR}/oZKS/hash
)

set(OZKS_SOURCE_FILES ${OZKS_SOURCE_FILES} PARENT_SCOPE)
