# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Set up a standalone build of the examples against an installed OZKS package. The root
# CMakeLists.txt defines these variables for in-tree builds; a standalone build has to
# define them itself before the example directories are added.
macro(ozks_standalone_setup)
    find_package(OZKS 2.0 REQUIRED)

    # The example schema compilers (CompileOZKSSchemaCXX.cmake and
    # CompileOZKSDistSchemaCXX.cmake) write their generated headers under
    # ${OZKS_BUILD_DIR}/oZKS and run flatc from FLATBUFFERS_FLATC_PATH. Resolve flatc in the
    # same order the schema compilers would: an explicit CUSTOM_FLATC_PATH, then the flatc
    # that belongs to the flatbuffers OZKS was built against (whichever configuration it
    # exports), then PATH as a last resort.
    set(OZKS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
    if(CUSTOM_FLATC_PATH)
        set(FLATBUFFERS_FLATC_PATH ${CUSTOM_FLATC_PATH})
    endif()
    if(NOT FLATBUFFERS_FLATC_PATH AND TARGET flatbuffers::flatc)
        get_target_property(_ozks_flatc_configs flatbuffers::flatc IMPORTED_CONFIGURATIONS)
        if(_ozks_flatc_configs)
            foreach(_ozks_flatc_config IN LISTS _ozks_flatc_configs)
                get_target_property(
                    FLATBUFFERS_FLATC_PATH flatbuffers::flatc
                    IMPORTED_LOCATION_${_ozks_flatc_config})
                if(FLATBUFFERS_FLATC_PATH)
                    break()
                endif()
            endforeach()
        endif()
        if(NOT FLATBUFFERS_FLATC_PATH)
            get_target_property(FLATBUFFERS_FLATC_PATH flatbuffers::flatc IMPORTED_LOCATION)
        endif()
        unset(_ozks_flatc_configs)
        unset(_ozks_flatc_config)
    endif()
    if(NOT FLATBUFFERS_FLATC_PATH)
        find_program(FLATBUFFERS_FLATC_PATH flatc)
    endif()
    if(NOT FLATBUFFERS_FLATC_PATH)
        message(FATAL_ERROR "flatc: not found (set CUSTOM_FLATC_PATH or add flatc to PATH)")
    endif()

    # Output layout and install destinations, as the root build defines them.
    set(OUTLIB_PATH "lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${OUTLIB_PATH})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${OUTLIB_PATH})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    include(GNUInstallDirs)
    set(OZKS_INCLUDES_INSTALL_DIR
        ${CMAKE_INSTALL_INCLUDEDIR}/OZKS-${OZKS_VERSION_MAJOR}.${OZKS_VERSION_MINOR})
endmacro()
