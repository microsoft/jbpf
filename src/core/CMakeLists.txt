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
set(JBPF_LIB_TARGETS ${JBPF_LIB})

if(JBPF_STATIC)
    add_library(${JBPF_LIB} STATIC ${JBPF_LIB_SOURCES} ${JBPF_LIB_HEADER_FILES}
                $<TARGET_OBJECTS:jbpf::logger_lib> 
                $<TARGET_OBJECTS:jbpf::io_lib> 
                $<TARGET_OBJECTS:jbpf::mem_mgmt_lib> 
                $<TARGET_OBJECTS:jbpf::lcm_ipc_lib>)

    target_link_libraries(${JBPF_LIB} libubpf 
                                        ${JBPF_CK_STATIC_LIB} 
                                        mimalloc-static
                                        pthread 
                                        dl 
                                        rt)
endif()

if(NOT JBPF_STATIC OR JBPF_STATIC STREQUAL "both")
    set(JBPF_SHARED_LIB ${JBPF_LIB})
    if(JBPF_STATIC STREQUAL "both")
        set(JBPF_SHARED_LIB ${JBPF_LIB}_shared)
        list(APPEND JBPF_LIB_TARGETS ${JBPF_SHARED_LIB})
    endif()
    add_library(${JBPF_SHARED_LIB} SHARED ${JBPF_LIB_SOURCES} ${JBPF_LIB_HEADER_FILES})
    set_target_properties(${JBPF_SHARED_LIB} PROPERTIES
        OUTPUT_NAME ${JBPF_LIB}
        LINK_FLAGS "-Wl,-z,now -Wl,--version-script=${JBPF_HELPER_BUILD_FILES}/symver.map"
    )
    target_compile_definitions(${JBPF_SHARED_LIB} PRIVATE JBPF_SHARED_LIB)
    target_link_libraries(${JBPF_SHARED_LIB} PUBLIC jbpf::lcm_ipc_lib
                                                jbpf::logger_lib
                                                jbpf::io_lib
                                                jbpf::mem_mgmt_lib
                                                libubpf 
                                                ${JBPF_CK_STATIC_LIB} 
                                                mimalloc-static
                                                pthread 
                                                dl 
                                                rt)
    target_compile_options(${JBPF_SHARED_LIB} PUBLIC -flto)
endif()
add_library(jbpf::core_lib ALIAS ${JBPF_LIB})
foreach(JBPF_LIB_TARGET IN LISTS JBPF_LIB_TARGETS)
    target_include_directories(${JBPF_LIB_TARGET} PUBLIC ${JBPF_LIB_HEADER_FILES}
                                                    ${JBPF_COMMON_HEADERS}
                                                    ${JBPF_IO_HEADER_FILES}
                                                    ${JBPF_LCM_IPC_HEADER_FILES}
                                                    ${JBPF_LOGGER_HEADERS}
                                                    ${JBPF_CK_INCLUDES})

    add_dependencies(${JBPF_LIB_TARGET} jbpf_libck libubpf generate_jbpf_lib_version copy-common-headers)
    add_clang_format_check(${JBPF_LIB_TARGET} ${JBPF_LIB_SOURCES})
    add_cppcheck(${JBPF_LIB_TARGET} ${JBPF_LIB_SOURCES})

    set_target_properties(${JBPF_LIB_TARGET}
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    )
endforeach()

add_custom_command(TARGET ${JBPF_LIB} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_device_defs.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_config.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_hook_defs.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_hook.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_perf_ext.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_perf.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_utils.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_helper.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_LIB_DIR}/jbpf_helper_api_defs.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${JBPF_CK_INCLUDES} ${OUTPUT_DIR}/inc/
)
