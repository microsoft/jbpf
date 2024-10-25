## codelet tests
set(VERIFIER_EXTENSIONS_TESTS ${TESTS_FUNCTIONAL}/verifier_extensions)

file(GLOB VERIFIER_EXTENSIONS_TESTS_CPP_SOURCES ${VERIFIER_EXTENSIONS_TESTS}/*.cpp)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)

# Loop through each test file and create an executable
foreach(TEST_FILE ${VERIFIER_EXTENSIONS_TESTS_CPP_SOURCES})
  # Get the filename without the path
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

  # Create an executable target for the test
  add_executable(${TEST_NAME} ${TEST_FILE})

  # Link the necessary libraries
  target_link_libraries(${TEST_NAME} PUBLIC jbpf::core_lib PUBLIC jbpf::verifier_lib)

  # Set the include directories
  target_include_directories(${TEST_NAME} PUBLIC ${JBPF_LIB_HEADER_FILES} ${JBPF_VERIFIER_HEADERS} ${TEST_HEADER_FILES})

  # Add the test to the list of tests to be executed
  add_test(NAME verifier_extensions/${TEST_NAME} COMMAND ${TEST_NAME})

  # Test coverage
  list(APPEND JBPF_TESTS verifier_extensions/${TEST_NAME})
  add_clang_format_check(${TEST_NAME} ${TEST_FILE})
  add_cppcheck(${TEST_NAME} ${TEST_FILE})
  set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
endforeach()
