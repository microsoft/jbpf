cmake_minimum_required(VERSION 3.16)

# Project name
project(ProxyProject LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_FLAGS "-std=c++17")

# Binary name
set(BINARY_NAME proxy)
set(LIBRARY_NAME reverse_proxy)

# Include directories
include_directories(
    $ENV{JBPF_PATH}/src/io
    $ENV{JBPF_PATH}/src/common
    $ENV{JBPF_PATH}/src/lcm
    $ENV{JBPF_PATH}/src/mem_mgmt
    $ENV{JBPF_PATH}/tools/lcm_cli
    $ENV{JBPF_PATH}/src/verifier
    $ENV{JBPF_PATH}/3p/ebpf-verifier/src
)

# Linker flags
set(LINK_LIBS
    jbpf_lcm
    jbpf
    boost_program_options
    jbpf_verifier
    ck
    mimalloc
    pthread
    dl
    rt
)

list(APPEND LINK_LIBS libubpf)

link_directories($ENV{JBPF_OUT_DIR}/lib)

# Source files for the library (everything except main.cpp)
set(LIB_SOURCES
    parser.cpp listener.cpp
    $ENV{JBPF_PATH}/tools/lcm_cli/stream_id.cpp
)

# Create a static library from the source files
add_library(${LIBRARY_NAME} STATIC ${LIB_SOURCES})

# store library in the current directory
set_target_properties(${LIBRARY_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}"
    LIBRARY_OUTPUT_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Add executable for main.cpp and link with the static library
add_executable(${BINARY_NAME} main.cpp)

# store executable in the current directory
set_target_properties(${BINARY_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link the static library and additional libraries with the executable
target_link_libraries(${BINARY_NAME} ${LIBRARY_NAME} ${LINK_LIBS} ${LINK_FLAGS})

add_library(jbpf::reverse_proxy_lib ALIAS ${LIBRARY_NAME})