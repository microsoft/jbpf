# Copyright (c) Microsoft Corporation. All rights reserved.

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(TESTS_CONCURRENCY ${TESTS_BASE}/concurrency)

## add tests from subdirectories
add_subdirectory(mem)
add_subdirectory(hashmap)
add_subdirectory(control_input)
add_subdirectory(ringbuf)
add_subdirectory(hooks)
add_subdirectory(atomic)

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
