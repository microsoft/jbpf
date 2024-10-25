cmake_minimum_required(VERSION 3.16)

project(jbpf_lcm_ipc_cli)

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

set(JBPF_LCM_IPC_CLI jbpf_lcm_cli)

set(JBPF_LCM_IPC_CLI_SRC_DIR ${PROJECT_SOURCE_DIR} PARENT_SCOPE)

set(JBPF_LCM_IPC_CLI_SOURCES ${PROJECT_SOURCE_DIR}/stream_id.cpp
                                ${PROJECT_SOURCE_DIR}/parser.cpp
                                ${PROJECT_SOURCE_DIR}/loader.cpp
                                ${PROJECT_SOURCE_DIR}/main.cpp)

set(JBPF_LCM_IPC_CLI_HEADER_FILES ${PROJECT_SOURCE_DIR} PARENT_SCOPE)

add_executable(${JBPF_LCM_IPC_CLI} ${JBPF_LCM_IPC_CLI_SOURCES})
target_include_directories(${JBPF_LCM_IPC_CLI} PUBLIC ${JBPF_LCM_HEADER_FILES}
                                                        ${HEADER_FILES}
                                                        ${JBPF_LCM_IPC_CLI_HEADER_FILES}
                                                        ${JBPF_VERIFIER_HEADERS})
target_link_libraries(${JBPF_LCM_IPC_CLI} PUBLIC jbpf::core_lib jbpf::verifier_lib ${YAML_CPP_LIBRARIES} jbpf::lcm_ipc_lib)

set_target_properties(${JBPF_LCM_IPC_CLI}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/bin"
)