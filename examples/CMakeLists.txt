add_custom_target(examples ALL
                  COMMAND BUILD_TYPE=$<CONFIG> JBPF_OUT_DIR=$ENV{JBPF_OUT_DIR} JBPF_PATH=$ENV{JBPF_PATH} make all
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


                
add_dependencies(examples jbpf::core_lib)

add_subdirectory(reverse_proxy)
