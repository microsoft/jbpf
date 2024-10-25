# Copyright (c) Microsoft Corporation. All rights reserved.
cmake_minimum_required(VERSION 3.16)

project(jbpf_io)

set(JBPF_IO_LIB jbpf_io)

SET(CMAKE_C_FLAGS "-fPIC")
add_link_options(-Wl,--no-as-needed -ldl)

set(JBPF_IO_SRC_DIR ${PROJECT_SOURCE_DIR})


if(JBPF_THREADS_LARGE)
add_definitions(-DJBPF_IO_MAX_NUM_THREADS=256)
endif(JBPF_THREADS_LARGE)

set(JBPF_IO_SOURCES ${JBPF_IO_SRC_DIR}/jbpf_io.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_ipc.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_utils.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_queue.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_thread_mgmt.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_channel.c
                    ${JBPF_IO_SRC_DIR}/jbpf_io_local.c
)

set(JBPF_IO_HEADER_FILES ${JBPF_IO_SRC_DIR} PARENT_SCOPE)


add_library(${JBPF_IO_LIB} ${JBPF_IO_SOURCES} $<TARGET_OBJECTS:jbpf::mem_mgmt_lib> $<TARGET_OBJECTS:jbpf::logger_lib>)
add_library(jbpf::io_lib ALIAS ${JBPF_IO_LIB})
target_link_libraries(${JBPF_IO_LIB} PUBLIC mimalloc-static ${JBPF_CK_STATIC_LIB} ${CMAKE_DL_LIBS})
target_include_directories(${JBPF_IO_LIB} PUBLIC ${JBPF_COMMON_HEADERS} ${JBPF_IO_HEADER_FILES} ${JBPF_LOGGER_HEADERS} ${MIMALLOC_HEADERS} ${JBPF_MEM_MGMT_HEADER_FILES} ${JBPF_CK_INCLUDES})

set_target_properties(${JBPF_IO_LIB}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/lib"
)

add_dependencies(${JBPF_IO_LIB} jbpf_libck)

add_custom_command(TARGET ${JBPF_IO_LIB} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/ 
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_IO_SRC_DIR}/jbpf_io.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_IO_SRC_DIR}/jbpf_io_defs.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_IO_SRC_DIR}/jbpf_io_channel_defs.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_IO_SRC_DIR}/jbpf_io_channel.h ${OUTPUT_DIR}/inc/
)

add_clang_format_check(${JBPF_IO_LIB} ${JBPF_IO_SOURCES})
add_cppcheck(${JBPF_IO_LIB} ${JBPF_IO_SOURCES})