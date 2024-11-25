# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_verifier)

set(JBPF_VERIFIER_LIB jbpf_verifier)
set(JBPF_VERIFIER_CLI jbpf_verifier_cli)

set(JBPF_VERIFIER_SRC_DIR ${PROJECT_SOURCE_DIR})

set(JBPF_VERIFIER_SOURCES ${JBPF_VERIFIER_SRC_DIR}/jbpf_platform.cpp
                            ${JBPF_VERIFIER_SRC_DIR}/jbpf_verifier.cpp
                            ${JBPF_VERIFIER_SRC_DIR}/specs/jbpf_prototypes.cpp)

set(JBPF_VERIFIER_CLI_SOURCES ${JBPF_VERIFIER_SRC_DIR}/jbpf_verifier_cli.cpp)


set(JBPF_VERIFIER_HEADERS ${JBPF_VERIFIER_SRC_DIR}
                            ${JBPF_VERIFIER_SRC_DIR}/specs PARENT_SCOPE)


######## jbpf verifier library ########
add_library(${JBPF_VERIFIER_LIB} ${JBPF_VERIFIER_SOURCES} $<TARGET_OBJECTS:ebpfverifier> $<TARGET_OBJECTS:libbtf>)
add_library(jbpf::verifier_lib ALIAS ${JBPF_VERIFIER_LIB})
target_include_directories(${JBPF_VERIFIER_LIB} PUBLIC ${JBPF_VERIFIER_HEADERS} 
                                                        ${EBPF_VERIFIER_HEADERS}
                                                        ${JBPF_COMMON_HEADERS})

set_target_properties(${JBPF_VERIFIER_LIB}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
)

######## jbpf verifier cli ########
add_executable(${JBPF_VERIFIER_CLI} ${JBPF_VERIFIER_CLI_SOURCES})
target_include_directories(${JBPF_VERIFIER_CLI} PUBLIC ${JBPF_VERIFIER_HEADERS})
target_link_libraries(${JBPF_VERIFIER_CLI} ${JBPF_VERIFIER_LIB})

set_target_properties(${JBPF_VERIFIER_CLI}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/bin"
)

add_custom_command(TARGET ${JBPF_VERIFIER_LIB} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/ 
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_VERIFIER_SRC_DIR}/jbpf_verifier.hpp ${OUTPUT_DIR}/inc/  
)

add_dependencies(${JBPF_VERIFIER_LIB} ebpfverifier copy-verifier-headers)

add_clang_format_check(${JBPF_VERIFIER_LIB} ${JBPF_VERIFIER_SOURCES})
add_cppcheck(${JBPF_VERIFIER_LIB} ${JBPF_VERIFIER_SOURCES})