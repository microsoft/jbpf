project(jbpf_emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_FLAGS "-std=c++17")

set(JBPF_EMULATOR jbpf_emulator)
set(JBPF_EMULATOR_SRC_DIR ${PROJECT_SOURCE_DIR})
set(JBPF_EMULATOR_SOURCES ${JBPF_EMULATOR_SRC_DIR}/emulator.cpp)

## require ENV JBPF_OUT_DIR
if(NOT DEFINED ENV{JBPF_OUT_DIR})
    message(FATAL_ERROR "Please set the environment variable JBPF_OUT_DIR to the output directory")
endif()

set(JBPF_OUT_DIR $ENV{JBPF_OUT_DIR})

## require ENV JBPF_PATH
if(NOT DEFINED ENV{JBPF_PATH})
    message(FATAL_ERROR "Please set the environment variable JBPF_PATH to the jbpf directory")
endif()

set(JBPF_PATH $ENV{JBPF_PATH})

find_package(Python COMPONENTS Interpreter Development REQUIRED)

message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")
message(STATUS "Python_VERSION: ${Python_VERSION}")
message(STATUS "Python_INCLUDE_DIRS: ${Python_INCLUDE_DIRS}")
message(STATUS "Python_LIBRARIES: ${Python_LIBRARIES}")
set(Python3_LINK_OPTIONS "-lpython${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
message(STATUS "Python3_LINK_OPTIONS: ${Python3_LINK_OPTIONS}")

include_directories(${Python_INCLUDE_DIRS})

add_executable(${JBPF_EMULATOR} ${JBPF_EMULATOR_SOURCES})
target_include_directories(${JBPF_EMULATOR} PUBLIC 
    ${JBPF_EMULATOR_HEADERS} 
    ${JBPF_LIB_HEADER_FILES}
    ${JBPF_IO_HEADER_FILES}
    ${JBPF_MEM_MGMT_HEADER_FILES}
    ${JBPF_LCM_IPC_HEADER_FILES}
    ${JBPF_COMMON_HEADERS}
    ${JBPF_CK_HEADER_FILES}
    ${JBPF_CK_INCLUDES}
    ${JBPF_OUT_DIR}/inc
    ${CMAKE_SOURCE_DIR}/jbpf_tests/common
    ${UBPF_INCLUDE_DIRS}
    ${JBPF_LOGGER_HEADERS}
)
set_target_properties(${JBPF_EMULATOR} PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(${JBPF_EMULATOR} jbpf jbpf_verifier jbpf_io jbpf_mem_mgmt jbpf_lcm)

target_link_options(${JBPF_EMULATOR} PUBLIC ${Python3_LINK_OPTIONS} "-lutil" "-lm" "-lpthread" "-ldl" "-lrt")

## link to libjbpf.a or libjbpf.so
if (JBPF_STATIC)
    target_link_libraries(${JBPF_EMULATOR}
        ${JBPF_OUT_DIR}/lib/libjbpf_io.a
        ${JBPF_OUT_DIR}/lib/libjbpf.a
        ${JBPF_OUT_DIR}/lib/libjbpf_mem_mgmt.a
        ${JBPF_OUT_DIR}/lib/libjbpf_verifier.a
        ${JBPF_OUT_DIR}/lib/libjbpf_lcm.a
        ${JBPF_OUT_DIR}/lib/libubpf.a
        ${JBPF_OUT_DIR}/lib/libck.a
        ${JBPF_CK_STATIC_LIB} pthread dl rt ${Python_LIBRARIES}
    )
else()
    target_link_libraries(${JBPF_EMULATOR}
        ${JBPF_OUT_DIR}/lib/libjbpf_io.a
        ${JBPF_OUT_DIR}/lib/libjbpf.so
        ${JBPF_OUT_DIR}/lib/libjbpf_mem_mgmt.a
        ${JBPF_OUT_DIR}/lib/libjbpf_verifier.a
        ${JBPF_OUT_DIR}/lib/libjbpf_lcm.a
        ${JBPF_OUT_DIR}/lib/libubpf.a
        ${JBPF_OUT_DIR}/lib/libck.a
        ${JBPF_CK_STATIC_LIB} pthread dl rt ${Python_LIBRARIES}
    )
endif()    

add_clang_format_check(${JBPF_EMULATOR} ${JBPF_EMULATOR_SOURCES})
#add_cppcheck(${JBPF_EMULATOR} ${JBPF_EMULATOR_SOURCES})

set_target_properties(${JBPF_EMULATOR}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY  "${OUTPUT_DIR}/bin"
)

add_custom_command(TARGET ${JBPF_EMULATOR} POST_BUILD
    COMMAND ctypesgen -a ../lcm/jbpf_lcm_api.h --no-macro-warnings -I ../io -I ../mem_mgmt -I ../common -o ${OUTPUT_DIR}/lib/jbpf_lcm_api.py
    COMMAND python3 ${PROJECT_SOURCE_DIR}/patch.py ${OUTPUT_DIR}/lib/jbpf_lcm_api.py
    COMMAND ctypesgen -a ../../jbpf_tests/common/jbpf_test_def.h --no-macro-warnings -I ../io -I ../mem_mgmt -I ../common -o ${OUTPUT_DIR}/lib/jbpf_test_def.py
    COMMAND python3 ${PROJECT_SOURCE_DIR}/patch.py ${OUTPUT_DIR}/lib/jbpf_test_def.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building python APIs for tests using the C headers (jbpf_lcm_api.h)..."
)

## copy the source files to the out/emulator directory
add_custom_command(TARGET ${JBPF_EMULATOR} POST_BUILD
    ## make out/emulator directory
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/emulator
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/emulator/test
    ## minimal emulator files
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/emulator_utils.py ${OUTPUT_DIR}/emulator/
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/patch.py ${OUTPUT_DIR}/emulator/
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/emulator.cpp ${OUTPUT_DIR}/emulator/
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/hook_wrapper.h ${OUTPUT_DIR}/emulator/
    ## example test files
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/test/* ${OUTPUT_DIR}/emulator/test
    ## hook header files
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_PATH}/jbpf_tests/common/jbpf_test_def.h ${OUTPUT_DIR}/emulator/test
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_PATH}/src/lcm/jbpf_lcm_api.h ${OUTPUT_DIR}/emulator/test
    ## copy CMakeLists_emulator.txt to out/emulator/CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E copy ${JBPF_EMULATOR_SRC_DIR}/CMakeLists_emulator.txt ${OUTPUT_DIR}/emulator/CMakeLists.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "copy the source files to the out/emulator directory..."
)
