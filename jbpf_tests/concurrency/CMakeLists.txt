# Copyright (c) Microsoft Corporation. All rights reserved.

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(TESTS_CONCURRENCY ${TESTS_BASE}/concurrency)

## add tests from subdirectories
add_subdirectory(mem)
add_subdirectory(hashmap)
add_subdirectory(control_input)
add_subdirectory(ringbuf)
add_subdirectory(hooks)
add_subdirectory(channel)

# The ARM version of ubpf does not support atomic operations.
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  message(STATUS "Skipping tests in concurrency/atomic. Atomic operations in ubpf are not supported on aarch64.")
else()
  add_subdirectory(atomic)
endif() 

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
