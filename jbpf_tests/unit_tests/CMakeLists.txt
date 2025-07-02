# Copyright (c) Microsoft Corporation. All rights reserved.

set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
add_subdirectory(validate_string_param)
add_subdirectory(bitmap)
add_subdirectory(mem)
add_subdirectory(io_mem)
add_subdirectory(array)
add_subdirectory(helper_functions)
add_subdirectory(hashmap)
set(JBPF_TESTS ${JBPF_TESTS} PARENT_SCOPE)
set(JBPF_STATIC_OPTION_UPPER ${JBPF_STATIC_OPTION_UPPER} PARENT_SCOPE)