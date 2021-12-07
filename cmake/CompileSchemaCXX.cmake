# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

if(CUSTOM_FLATC_PATH)
    set(FLATBUFFERS_FLATC_PATH ${CUSTOM_FLATC_PATH})
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/commitment.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile commitment.fbs (${result})")
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/compressed_trie.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile compressed_trie.fbs (${result})")
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/ct_node.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile ct_node.fbs (${result})")
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/ozks.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile ozks.fbs (${result})")
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/ozks_store.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile ozks.fbs (${result})")
endif()

execute_process(
    COMMAND ${FLATBUFFERS_FLATC_PATH} --cpp -o "${OZKS_BUILD_DIR}/oZKS" "${OZKS_SOURCE_DIR}/oZKS/query_result.fbs"
    OUTPUT_QUIET
    RESULT_VARIABLE result)
if(result)
    message(FATAL_ERROR "flatc failed to compile query_result.fbs (${result})")
endif()
