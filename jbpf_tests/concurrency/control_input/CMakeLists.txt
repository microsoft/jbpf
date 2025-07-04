# Copyright (c) Microsoft Corporation. All rights reserved.
## control input concurrency tests
set(CONTROL_INPUT_CONCURRENCY_TESTS ${TESTS_CONCURRENCY}/control_input)
file(GLOB CONTROL_INPUT_CONCURRENCY_TESTS_SOURCES ${CONTROL_INPUT_CONCURRENCY_TESTS}/*.c)

# The ARM version of ubpf does not support atomic operations, so exclude codelet_control_input_concurrency_atomic_test for aarch64
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  message(STATUS "Skipping test concurrency/control_input/codelet_control_input_concurrency_atomic_test. Atomic operations in ubpf not supported on aarch64.")
  list(REMOVE_ITEM CONTROL_INPUT_CONCURRENCY_TESTS_SOURCES ${CONTROL_INPUT_CONCURRENCY_TESTS}/codelet_control_input_concurrency_atomic_test.c)
endif()

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
# Loop through each test file and create an executable
foreach(TEST_FILE ${CONTROL_INPUT_CONCURRENCY_TESTS_SOURCES})
  # Get the filename without the path
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

  # Create an executable target for the test
  add_executable(${TEST_NAME} ${TEST_FILE} ${TESTS_COMMON}/jbpf_test_lib.c ${JBPF_HASHMAP_MGMT_SOURCES}) 

  # Link the necessary libraries
  target_link_libraries(${TEST_NAME} PUBLIC jbpf::core_lib jbpf::logger_lib jbpf::mem_mgmt_lib)

  # Set the include directories
  target_include_directories(${TEST_NAME} PUBLIC ${JBPF_LIB_HEADER_FILES} ${TEST_HEADER_FILES} ${JBPF_HASHMAP_MGMT_HEADER_FILES})

  # Add the test to the list of tests to be executed
  add_test(NAME concurrency/${TEST_NAME} COMMAND ${TEST_NAME})

  # Test coverage
  list(APPEND JBPF_TESTS concurrency/${TEST_NAME})
  add_clang_format_check(${TEST_NAME} ${TEST_FILE})
  add_cppcheck(${TEST_NAME} ${TEST_FILE})
  set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
endforeach()
