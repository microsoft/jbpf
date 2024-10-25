# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_lcm_ipc)

set(JBPF_LCM_IPC_LIB jbpf_lcm)

set(JBPF_LCM_IPC_SRC_DIR ${PROJECT_SOURCE_DIR})

set(JBPF_LCM_IPC_SOURCES ${JBPF_LCM_IPC_SRC_DIR}/jbpf_lcm_ipc.c)

set(JBPF_LCM_IPC_HEADER_FILES ${JBPF_LCM_IPC_SRC_DIR} PARENT_SCOPE)


add_library(${JBPF_LCM_IPC_LIB} ${JBPF_LCM_IPC_SOURCES})
add_library(jbpf::lcm_ipc_lib ALIAS ${JBPF_LCM_IPC_LIB})
target_link_libraries(${JBPF_LCM_IPC_LIB} PUBLIC jbpf::logger_lib)
target_include_directories(${JBPF_LCM_IPC_LIB} PUBLIC ${JBPF_LCM_IPC_HEADER_FILES} 
                                                        ${JBPF_IO_HEADER_FILES} 
                                                        ${JBPF_MEM_MGMT_HEADER_FILES}
                                                        ${JBPF_COMMON_HEADERS}
                                                        ${JBPF_LOGGER_HEADERS})
                                                        
add_custom_command(TARGET ${JBPF_LCM_IPC_LIB} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/ 
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LCM_IPC_SRC_DIR}/jbpf_lcm_api.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LCM_IPC_SRC_DIR}/jbpf_lcm_ipc.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LCM_IPC_SRC_DIR}/jbpf_lcm_ipc_msg.h ${OUTPUT_DIR}/inc/
)

set_target_properties(${JBPF_LCM_IPC_LIB}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
)
