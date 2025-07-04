cmake_minimum_required(VERSION 3.16)

include(ExternalProject)
project(jbpf-tests-functional)
include(CTest)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)

#set(JBPF_CONCURRENCY_TEST jbpf_concurrency_test)
set(JBPF_AGENT_CPP_TEST jbpf_agent_cpp_test)
set(JBPF_SYSTEM_TEST jbpf_system_test)
set(JBPF_UNIT_TEST jbpf_unit_test)
set(JBPF_BASIC_UNIT_TEST jbpf_basic_unit_test)
set(TESTS_FUNCTIONAL ${TESTS_BASE}/functional)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)

## agent cpp
set(JBPF_AGENT_CPP_SOURCES ${TESTS_FUNCTIONAL}/jbpf_agent.cpp ${TESTS_COMMON}/jbpf_agent_common.c)
add_executable(${JBPF_AGENT_CPP_TEST} ${JBPF_AGENT_CPP_SOURCES})
target_link_libraries(${JBPF_AGENT_CPP_TEST} PUBLIC jbpf::core_lib)
target_include_directories(${JBPF_AGENT_CPP_TEST} PUBLIC ${JBPF_CK_INCLUDES} ${JBPF_LIB_HEADER_FILES} ${TEST_HEADER_FILES})
list(APPEND JBPF_TESTS ${JBPF_AGENT_CPP_TEST})
add_clang_format_check(${JBPF_AGENT_CPP_TEST} ${JBPF_AGENT_CPP_SOURCES})
add_cppcheck(${JBPF_AGENT_CPP_TEST} ${JBPF_AGENT_CPP_SOURCES})

add_subdirectory(request_validation)
add_subdirectory(codeletSets)
add_subdirectory(codelets)
add_subdirectory(io)
add_subdirectory(load_unload)
add_subdirectory(verifier_extensions)
add_subdirectory(ctrl_hooks)
add_subdirectory(helper_functions)
add_subdirectory(array)
add_subdirectory(perf)


