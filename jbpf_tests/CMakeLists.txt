cmake_minimum_required(VERSION 3.16)

include(ExternalProject)
project(jbpf-tests)
include(CTest)

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -UNDEBUG")

### Test Files
set(TEST_FILES test_files)
set(TESTS_BASE ${PROJECT_SOURCE_DIR})
set(TESTS_COMMON ${TESTS_BASE}/common)
set(TEST_HEADER_FILES ${TESTS_COMMON})

add_subdirectory(${TEST_FILES}/serde_test_libs)

### Add tests
add_subdirectory(e2e_examples)
add_subdirectory(functional)
add_subdirectory(tools/lcm_cli)
add_subdirectory(verifier)
add_subdirectory(unit_tests)
add_subdirectory(stress_tests)
add_subdirectory(concurrency)

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
