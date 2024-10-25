# Copyright (c) Microsoft Corporation. All rights reserved.

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(TESTS_STRESS_BASE ${TESTS_BASE}/stress_tests)
add_subdirectory(io)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
