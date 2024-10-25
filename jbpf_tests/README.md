# Writing Tests
## Folder Structure
The tests are organized into several folders based on their purpose:

- common: Contains common files and utilities used across all tests.
- e2e_examples: Contains end-to-end example tests.
- functional: Contains blackbox tests, organized by feature or functionality. These tests require jbpf to run.
- test_files: Contains test files, such as prebuilt .o codelets.
- unit_tests: Contains unit tests for individual functions or modules.
- benchmarks: Contains performance benchmarks tests.
- stress: Contains stress tests.

## Adding New Tests
To add a new set of tests, follow these steps:

- Create a New Folder: In the appropriate directory (e.g., unit_tests, functional, e2e_examples), create a new folder for your test set.
- Add CMakeLists.txt: Create a CMakeLists.txt file in your new folder. Here's a template:

```cmake
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(A_NEW_TEST a_new_test)
set(A_NEW_TEST_SOURCE a_new_test.c ${TESTS_COMMON}/jbpf_test_lib.c)
add_executable(${A_NEW_TEST} ${A_NEW_TEST_SOURCE})
target_link_libraries(${A_NEW_TEST} PUBLIC ${JBPF_LIB})
target_include_directories(${A_NEW_TEST} PUBLIC ${HEADER_FILES} ${TEST_HEADER_FILES})
add_test(NAME ${A_NEW_TEST} COMMAND ${A_NEW_TEST})
list(APPEND JBPF_TESTS ${A_NEW_TEST})
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
add_clang_format_check(${A_NEW_TEST} ${A_NEW_TEST_SOURCE})
add_cppcheck(${A_NEW_TEST} ${A_NEW_TEST_SOURCE})
```

- Update Parent CMakeLists.txt: In the parent directory's CMakeLists.txt, add a reference to your new test directory:
```cmake
add_subdirectory(a_new_test)
```

## Guidelines for Writing Tests
### Complex Tests
- Location: Place complex tests, e.g. black box tests that require jbpf, in a single test file.
- Example: [jbpf_e2e_ipc_test.c](e2e_examples/jbpf_e2e_ipc_test.c).

### Small Unit Tests
- Location: Place small unit tests in the [unit_tests](unit_tests) directory. Use the lightweight testing library located at [common/jbpf_test_lib.c](common/jbpf_test_lib.c) to assist with writing these tests.
- Example: For unit tests of the `validate_string_param` function, use [unit_tests/validate_string_param/test.c](unit_tests/validate_string_param/test.c).

#### Test File Template
For unit tests, use the following template:

```c
#include "jbpf_test_lib.h"

void test_case_1() {
    // Test case implementation
}

void test_case_2() {
    // Test case implementation
}

int main(int argc, char** argv) {
    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_case_1, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_case_2, NULL, NULL, NULL),
        // Add more tests as needed
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}
```

This structure and guideline ensure a consistent and organized approach to adding and managing tests, making it easier to maintain and extend the test suite.
