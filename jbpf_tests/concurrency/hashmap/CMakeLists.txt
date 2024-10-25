# Copyright (c) Microsoft Corporation. All rights reserved.
## hashmap concurrency tests
set(HASHMAP_CONCURRENCY_TESTS ${TESTS_CONCURRENCY}/hashmap)
file(GLOB HASHMAP_CONCURRENCY_TESTS_SOURCES ${HASHMAP_CONCURRENCY_TESTS}/*.c)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
# Loop through each test file and create an executable
foreach(TEST_FILE ${HASHMAP_CONCURRENCY_TESTS_SOURCES})
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
