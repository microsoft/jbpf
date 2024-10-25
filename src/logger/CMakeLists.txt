# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_logger)

set(JBPF_LOGGER_LIB jbpf_logger)

set(JBPF_LOGGER_SRC_DIR ${PROJECT_SOURCE_DIR})

set(JBPF_LOGGER_SOURCES ${JBPF_LOGGER_SRC_DIR}/jbpf_logging.c)
set(JBPF_LOGGER_HEADERS ${JBPF_LOGGER_SRC_DIR} PARENT_SCOPE)

add_library(${JBPF_LOGGER_LIB} ${JBPF_LOGGER_SOURCES})
add_library(jbpf::logger_lib ALIAS ${JBPF_LOGGER_LIB})
target_include_directories(${JBPF_LOGGER_LIB} PUBLIC ${JBPF_LOGGER_HEADERS})
set_target_properties(${JBPF_LOGGER_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_clang_format_check(${JBPF_LOGGER_LIB} ${JBPF_LOGGER_SOURCES})
add_cppcheck(${JBPF_LOGGER_LIB} ${JBPF_LOGGER_SOURCES})