# Copyright (c) Microsoft Corporation. All rights reserved.

## bitmap tests
set(JBPF_BITMAP_TEST jbpf_bitmap_test)
set(JBPF_BITMAP_TESTS_SOURCE jbpf_bitmap_test.c ${TESTS_COMMON}/jbpf_test_lib.c)
add_executable(${JBPF_BITMAP_TEST} ${JBPF_BITMAP_TESTS_SOURCE})
if(JBPF_STATIC)
    target_link_libraries(${JBPF_BITMAP_TEST} PUBLIC jbpf::core_lib jbpf::logger_lib jbpf::mem_mgmt_lib)
else()
    target_link_libraries(${JBPF_BITMAP_TEST} PUBLIC jbpf::core_lib)
endif()
target_include_directories(${JBPF_BITMAP_TEST} PUBLIC ${JBPF_LIB_HEADER_FILES} ${TEST_HEADER_FILES} ${JBPF_LOGGER_HEADERS})
add_test(NAME unit_tests/${JBPF_BITMAP_TEST} COMMAND ${JBPF_BITMAP_TEST})
list(APPEND JBPF_TESTS unit_tests/${JBPF_BITMAP_TEST})
add_clang_format_check(${JBPF_BITMAP_TEST} ${JBPF_BITMAP_TESTS_SOURCE})
add_cppcheck(${JBPF_BITMAP_TEST} ${JBPF_BITMAP_TESTS_SOURCE})
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
