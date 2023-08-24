# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

if(CUSTOM_FLATC_PATH)
    set(FLATBUFFERS_FLATC_PATH ${CUSTOM_FLATC_PATH})
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKSDISTRIBUTED_SOURCE_FILES}/ozks_distributed.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile ozks_distributed.fbs (${result})")
endif()
