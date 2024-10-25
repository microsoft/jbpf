cmake_minimum_required(VERSION 3.16)

project(serde_test_libs)

set(TEST_SERDE_LIB test_serde)
set(TEST_SERDE2_LIB test_serde2)

set(SERDE_TEST_SRC_BASE ${PROJECT_SOURCE_DIR})

set(TEST_SERDE_LIB_SRC ${SERDE_TEST_SRC_BASE}/serde_lib_test_struct.c)
set(TEST_SERDE2_LIB_SRC ${SERDE_TEST_SRC_BASE}/serde_lib_test_struct2.c)

add_library(${TEST_SERDE_LIB} SHARED ${TEST_SERDE_LIB_SRC})
add_library(jbpf::test_serde ALIAS ${TEST_SERDE_LIB})
add_library(${TEST_SERDE2_LIB} SHARED ${TEST_SERDE2_LIB_SRC})
add_library(jbpf::test_serde2 ALIAS ${TEST_SERDE2_LIB})