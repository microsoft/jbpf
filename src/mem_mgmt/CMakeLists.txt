# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_mem_mgmt)

set(JBPF_MEM_MGMT_LIB jbpf_mem_mgmt)

if(CMAKE_BUILD_TYPE MATCHES AddressSanitizer)
    if (ENABLE_POISONING)
        add_definitions(-DJBPF_IO_POISON)
    endif()
endif()

add_link_options(-Wl,--no-as-needed -ldl)

set(JBPF_MEM_MGMT_DIR ${PROJECT_SOURCE_DIR})

set(JBPF_MEM_MGMT_SOURCES ${JBPF_MEM_MGMT_DIR}/jbpf_mem_mgmt.c
                           ${JBPF_MEM_MGMT_DIR}/jbpf_mempool.c
)
set(JBPF_MEM_MGMT_HEADER_FILES ${PROJECT_SOURCE_DIR} PARENT_SCOPE)

add_library(${JBPF_MEM_MGMT_LIB} ${JBPF_MEM_MGMT_SOURCES} $<TARGET_OBJECTS:mimalloc-static>)
add_library(jbpf::mem_mgmt_lib ALIAS ${JBPF_MEM_MGMT_LIB})
target_link_libraries(${JBPF_MEM_MGMT_LIB} PUBLIC mimalloc-static)
target_include_directories(${JBPF_MEM_MGMT_LIB} PUBLIC ${JBPF_MEM_MGMT_HEADER_FILES} ${JBPF_LOGGER_HEADERS} ${MIMALLOC_HEADERS} ${JBPF_CK_INCLUDES})

set_target_properties(${JBPF_MEM_MGMT_LIB}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
)

add_dependencies(${JBPF_MEM_MGMT_LIB} jbpf_libck)

add_custom_command(TARGET ${JBPF_MEM_MGMT_LIB} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/ 
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_MEM_MGMT_DIR}/jbpf_mempool.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_MEM_MGMT_DIR}/jbpf_mem_mgmt.h ${OUTPUT_DIR}/inc/
)

add_clang_format_check(${JBPF_MEM_MGMT_LIB} ${JBPF_MEM_MGMT_SOURCES})
add_cppcheck(${JBPF_MEM_MGMT_LIB} ${JBPF_MEM_MGMT_SOURCES})