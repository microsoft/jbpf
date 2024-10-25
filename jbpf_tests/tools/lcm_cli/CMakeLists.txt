include(ExternalProject)
project(jbpf-tests-tools-lcm-cli)

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

## lcm tests
file(GLOB LCM_CLI_TESTS_SOURCES ${TESTS_BASE}/tools/lcm_cli/*.cpp)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(TEST_LCM_FILES ${JBPF_LCM_IPC_CLI_SRC_DIR}/loader.cpp ${JBPF_LCM_IPC_CLI_SRC_DIR}/parser.cpp ${JBPF_LCM_IPC_CLI_SRC_DIR}/stream_id.cpp)

# Loop through each test file and create an executable
foreach(TEST_FILE ${LCM_CLI_TESTS_SOURCES})
  # Get the filename without the path
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

  # Create an executable target for the test
  add_executable(${TEST_NAME} ${TEST_FILE} ${TEST_LCM_FILES})

  # Link the necessary libraries
  target_link_libraries(${TEST_NAME} PUBLIC 
    ${YAML_CPP_LIBRARIES}
    jbpf::core_lib
    jbpf::verifier_lib
    jbpf::lcm_ipc_lib
    jbpf::logger_lib
    jbpf::mem_mgmt_lib
    jbpf::io_lib ${JBPF_CK_STATIC_LIB}
  --coverage)

  # Set the include directories
  target_include_directories(${TEST_NAME} PUBLIC ${TEST_HEADER_FILES} ${JBPF_LCM_IPC_HEADER_FILES} ${JBPF_LCM_IPC_CLI_HEADER_FILES})

  # Add the test to the list of tests to be executed
  add_test(NAME lcm_cli_tests/${TEST_NAME} COMMAND ${TEST_NAME})

  # Test coverage
  list(APPEND JBPF_TESTS lcm_cli_tests/${TEST_NAME})
  add_clang_format_check(${TEST_NAME} ${TEST_FILE})
  add_cppcheck(${TEST_NAME} ${TEST_FILE})
  set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
endforeach()
