# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_core)

set(JBPF_LIB jbpf)
set(JBPF_LIB_DIR ${PROJECT_SOURCE_DIR})

set(JBPF_LIB_SOURCES ${JBPF_LIB_DIR}/jbpf_helper_impl.c
                     ${JBPF_LIB_DIR}/jbpf_bpf_array.c
                     ${JBPF_LIB_DIR}/jbpf_bpf_hashmap.c
                     ${JBPF_LIB_DIR}/jbpf_bpf_spsc_hashmap.c
                     ${JBPF_LIB_DIR}/jbpf.c
                     ${JBPF_LIB_DIR}/jbpf_hook.c
                     ${JBPF_LIB_DIR}/jbpf_perf.c
                     ${JBPF_LIB_DIR}/jbpf_lookup3.c
                     ${JBPF_LIB_DIR}/jbpf_memory.c
                     ${JBPF_LIB_DIR}/jbpf_utils.c)

set(JBPF_LIB_HEADER_FILES ${PROJECT_SOURCE_DIR} PARENT_SCOPE)
set(JBPF_STATIC_OPTION_UPPER ${JBPF_STATIC_OPTION_UPPER} PARENT_SCOPE)

function(create_jbpf_library LIB_NAME LIBRARY_TYPE)
    if(LIBRARY_TYPE STREQUAL "static")
        add_library(${LIB_NAME} STATIC ${JBPF_LIB_SOURCES} ${JBPF_LIB_HEADER_FILES}
                    $<TARGET_OBJECTS:jbpf::logger_lib>
                    $<TARGET_OBJECTS:jbpf::io_lib>
                    $<TARGET_OBJECTS:jbpf::mem_mgmt_lib>
                    $<TARGET_OBJECTS:jbpf::lcm_ipc_lib>)
        target_link_libraries(${LIB_NAME} libubpf
                                          ${JBPF_CK_STATIC_LIB}
                                          mimalloc-static
                                          pthread
                                          dl
                                          rt)
    elseif(LIBRARY_TYPE STREQUAL "shared")
        add_library(${LIB_NAME} SHARED ${JBPF_LIB_SOURCES} ${JBPF_LIB_HEADER_FILES})
        target_link_libraries(${LIB_NAME} PUBLIC jbpf::lcm_ipc_lib
                                                jbpf::logger_lib
                                                jbpf::io_lib
                                                jbpf::mem_mgmt_lib
                                                libubpf
                                                ${JBPF_CK_STATIC_LIB}
                                                mimalloc-static
                                                pthread
                                                dl
                                                rt)
        target_compile_options(${LIB_NAME} PUBLIC -flto)
    endif()

    set_target_properties(${LIB_NAME} PROPERTIES
        LINK_FLAGS "-Wl,-z,now -Wl,--version-script=${JBPF_HELPER_BUILD_FILES}/symver.map"
        ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    )
    add_library(jbpf::core_lib ALIAS ${LIB_NAME})

    target_include_directories(${LIB_NAME} PUBLIC ${JBPF_LIB_HEADER_FILES}
                                                     ${JBPF_COMMON_HEADERS}
                                                     ${JBPF_IO_HEADER_FILES}
                                                     ${JBPF_LCM_IPC_HEADER_FILES}
                                                     ${JBPF_LOGGER_HEADERS}
                                                     ${JBPF_CK_INCLUDES})

    add_dependencies(${LIB_NAME} jbpf_libck libubpf generate_jbpf_lib_version)
    add_clang_format_check(${LIB_NAME} ${JBPF_LIB_SOURCES})
    add_cppcheck(${LIB_NAME} ${JBPF_LIB_SOURCES})

    add_custom_command(TARGET ${LIB_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_device_defs.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_config.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_hook_defs.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_hook.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_perf_ext.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_perf.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_utils.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_helper.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_LIB_DIR}/jbpf_helper_api_defs.h ${OUTPUT_DIR}/inc/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${JBPF_CK_INCLUDES} ${OUTPUT_DIR}/inc/
    )

    add_dependencies(${LIB_NAME} copy-common-headers)
endfunction()

message(STATUS "JBPF_STATIC_OPTION_UPPER: ${JBPF_STATIC_OPTION_UPPER}")

if(JBPF_STATIC_OPTION_UPPER STREQUAL "ON")
    message(STATUS "Building static library i.e. lib${JBPF_LIB}.a")
    create_jbpf_library(${JBPF_LIB} "static")
    add_library(jbpf::core_lib ALIAS ${JBPF_LIB})
elseif(JBPF_STATIC_OPTION_UPPER STREQUAL "OFF")
    message(STATUS "Building shared library i.e. lib${JBPF_LIB}.so")
    create_jbpf_library(${JBPF_LIB} "shared")
elseif(JBPF_STATIC_OPTION_UPPER STREQUAL "BOTH")
    message(STATUS "Building both static and shared libraries i.e. lib${JBPF_LIB}.a and lib${JBPF_LIB}.so")
    create_jbpf_library(${JBPF_LIB} "static")
    create_jbpf_library(${JBPF_LIB}_shared "shared")

    ## move libjbpf_shared.so to libjbpf.so
    add_custom_command(TARGET ${JBPF_LIB}_shared POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${JBPF_LIB}_shared> ${OUTPUT_DIR}/lib/lib${JBPF_LIB}.so
    )
endif()
