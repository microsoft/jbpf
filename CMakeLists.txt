cmake_minimum_required(VERSION 3.16)

include(ExternalProject)

project(jbpf)

option(USE_NATIVE "Do use -march=native compiler flag (disable to increase binary portability)" ON)
option(USE_JBPF_PERF_OPT "Assume CPU affinity when running jbpf hooks and use rdtsc for measuring time" ON)
option(USE_JBPF_PRINTF_HELPER "Enable printing from codelets using jbpf_printf_debug() helper function" ON)
option(JBPF_THREADS_LARGE "Allow more threads to be registered by jbpf and the IO lib" OFF)
option(ENABLE_POISONING "Enable ASAN poisoning. Should not be used for IPC mode tests and must be used in conjunction with ASAN" OFF)
option(JBPF_STATIC "Build jbpf as static library" OFF)
option(JBPF_EXPERIMENTAL_FEATURES "Enable experimental features of jbpf" OFF)
option(CLANG_FORMAT_CHECK "Enable clang-format check" OFF)
option(CPP_CHECK "Enable cppcheck" OFF)
option(BUILD_TESTING "Enable tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_FLAGS "-std=c++17")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

################ Setup env ##################

set(ENV{JBPF_PATH} ${PROJECT_SOURCE_DIR})

set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/out)
set(ENV{JBPF_OUT_DIR} ${OUTPUT_DIR})

file(MAKE_DIRECTORY ${OUTPUT_DIR})
file(MAKE_DIRECTORY ${OUTPUT_DIR}/inc)

set(JBPF_SOURCE_CODE ${PROJECT_SOURCE_DIR}/src)
set(JBPF_HELPER_BUILD_FILES ${PROJECT_SOURCE_DIR}/helper_build_files)
set(JBPF_TOOLS ${PROJECT_SOURCE_DIR}/tools)

set(VERSION_MAP_FILE ${JBPF_HELPER_BUILD_FILES}/symver.map)
set(VERSION_FILE ${PROJECT_SOURCE_DIR}/VERSION)
set(VERSION_HEADER ${JBPF_SOURCE_CODE}/common/jbpf_version.h)

# Execute the setup script first
option(INITIALIZE_SUBMODULES "Initialize submodules" ON)
if(INITIALIZE_SUBMODULES)
    execute_process(
        COMMAND ./init_and_patch_submodules.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Setup script failed with error: ${error}")
    endif()
endif()

################ Definitions ################

if(JBPF_THREADS_LARGE)
  add_definitions(-DJBPF_MAX_NUM_REG_THREADS=256)
endif(JBPF_THREADS_LARGE)

if(NOT JBPF_STATIC)
  add_definitions(-DJBPF_SHARED_LIB)
endif(NOT JBPF_STATIC)

# To build static library on ARM, disable out-of-line helpers for atomics 
if ((JBPF_STATIC) AND (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64"))
  add_compile_options(-mno-outline-atomics)
endif()

if(USE_JBPF_PERF_OPT)
  add_definitions(-DJBPF_PERF_OPT)
endif(USE_JBPF_PERF_OPT)

if(USE_JBPF_PRINTF_HELPER)
  add_definitions(-DJBPF_PRINTF_HELPER_ENABLED)
endif(USE_JBPF_PRINTF_HELPER)

if(JBPF_EXPERIMENTAL_FEATURES)
  add_definitions(-DJBPF_EXPERIMENTAL_FEATURES)
endif(JBPF_EXPERIMENTAL_FEATURES)

# Treat all warnings as errors except those explicitly called out in CPP using #warning
SET(CMAKE_C_WARNINGS "-Wall -Werror -Wno-missing-field-initializers -Wstrict-prototypes")

if(USE_NATIVE)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -march=native ${CMAKE_C_WARNINGS}")
else(USE_NATIVE)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 ${CMAKE_C_WARNINGS}")
endif(USE_NATIVE)

if(CMAKE_BUILD_TYPE MATCHES AddressSanitizer)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fsanitize=address -O1 -g -lasan -DJBPF_DEBUG_ENABLED")
  add_definitions(-DASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
elseif(CMAKE_BUILD_TYPE MATCHES Debug)
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O1 -DJBPF_DEBUG_ENABLED")
  add_compile_options(--coverage -g)
  add_link_options(--coverage)
else()
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_modules)


################ clang-format ################
# Define the clang-format executable
find_program(CLANG_FORMAT_BIN clang-format)

if (NOT CLANG_FORMAT_BIN)
  message(STATUS "clang-format executable not found thus skipping clang-format check")
endif()

# Function to create a clang-format check for a given target
function(add_clang_format_check TARGET_NAME TARGET_SOURCES)
  if (NOT CLANG_FORMAT_CHECK)
    return()
  endif()

  # Ensure clang-format binary is available
  if (CLANG_FORMAT_BIN)
    # Convert all relative paths in TARGET_SOURCES to absolute paths
    set(ABSOLUTE_SOURCES "")
    foreach(SOURCE ${TARGET_SOURCES})
      get_filename_component(ABSOLUTE_SOURCE ${SOURCE} ABSOLUTE)
      list(APPEND ABSOLUTE_SOURCES ${ABSOLUTE_SOURCE})
    endforeach()

    # Add a custom target for clang-format check
    add_custom_target(${TARGET_NAME}_clang_format_check
      COMMAND ${CMAKE_COMMAND} -E echo "Running clang-format check for ${TARGET_NAME}..."
      COMMAND ${CMAKE_SOURCE_DIR}/helper_build_files/clang-format-check.sh ${ABSOLUTE_SOURCES}
      COMMENT "Checking source code formatting with clang-format for ${TARGET_NAME}"
    )

    # Add a custom target to ensure checks are run before build
    add_custom_target(pre_build_checks_${TARGET_NAME}
      DEPENDS ${TARGET_NAME}_clang_format_check
    )

    # Add pre-build checks dependency to the target
    add_dependencies(${TARGET_NAME} pre_build_checks_${TARGET_NAME})
  endif()
endfunction()

################ cppcheck ################
# Function to add cppcheck for a given target
function(add_cppcheck TARGET_NAME TARGET_SOURCES)
  if (NOT CPP_CHECK)
    return()
  endif()

  # Convert all relative paths in TARGET_SOURCES to absolute paths
  set(ABSOLUTE_SOURCES "")
  foreach(SOURCE ${TARGET_SOURCES})
    get_filename_component(ABSOLUTE_SOURCE ${SOURCE} ABSOLUTE)
    list(APPEND ABSOLUTE_SOURCES ${ABSOLUTE_SOURCE})
  endforeach()

  add_custom_target(${TARGET_NAME}_cppcheck
    COMMAND ${CMAKE_COMMAND} -E echo "Running cppcheck for ${TARGET_NAME}..."
    COMMAND ${CMAKE_SOURCE_DIR}/helper_build_files/cppcheck.sh ${ABSOLUTE_SOURCES}
    COMMENT "Checking source code with cppcheck for ${TARGET_NAME}"
  )
  add_custom_target(pre_build_checks_cppcheck_${TARGET_NAME}
    DEPENDS ${TARGET_NAME}_cppcheck
  )
  add_dependencies(${TARGET_NAME} pre_build_checks_cppcheck_${TARGET_NAME})
endfunction()

################ libck ################

set(JBPF_CK_SRC ${PROJECT_SOURCE_DIR}/3p/ck)
set(JBPF_CK_BIN ${CMAKE_BINARY_DIR}/libck-jbpf)
set(JBPF_CK_STATIC_LIB ${JBPF_CK_BIN}/lib/libck.a)
set(JBPF_CK_INCLUDES ${JBPF_CK_BIN}/include)

ExternalProject_Add(jbpf_libck
    SOURCE_DIR ${JBPF_CK_SRC}
    CONFIGURE_COMMAND ${JBPF_CK_SRC}/configure --prefix=${JBPF_CK_BIN}
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1)

add_custom_command(TARGET jbpf_libck POST_BUILD
                  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/lib/ 
                  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_CK_STATIC_LIB} ${OUTPUT_DIR}/lib/)

################ Mimalloc ################
set(MI_OVERRIDE OFF)
set(MIMALLOC_DIR 3p/mimalloc)
set(MI_BUILD_TESTS OFF)
add_subdirectory(${MIMALLOC_DIR})
set(MIMALLOC_HEADERS ${MIMALLOC_DIR}/include)

add_custom_target(jbpf-mimalloc ALL DEPENDS mimalloc-static)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(MIMALLOC_LIB_NAME libmimalloc-debug.a)
elseif(CMAKE_BUILD_TYPE MATCHES AddressSanitizer)
  set(MIMALLOC_LIB_NAME libmimalloc-addresssanitizer.a)
else()
  set(MIMALLOC_LIB_NAME libmimalloc.a)
endif()

add_custom_command(TARGET jbpf-mimalloc POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/lib/ 
		    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_BINARY_DIR}/${MIMALLOC_DIR}/${MIMALLOC_LIB_NAME} ${OUTPUT_DIR}/lib/libmimalloc.a)

################ Ubpf ################
ExternalProject_Add(ubpf
    PREFIX ${CMAKE_BINARY_DIR}/ubpf
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/3p/ubpf
    CMAKE_ARGS
     -DUBPF_ENABLE_INSTALL=1
     -DCMAKE_INSTALL_INCLUDEDIR=${OUTPUT_DIR}/inc
     -DCMAKE_INSTALL_LIBDIR=${OUTPUT_DIR}/lib
)

list(APPEND UBPF_INCLUDE_DIRS ${OUTPUT_DIR}/inc)

# Create an imported target for the external library
add_library(libubpf STATIC IMPORTED)
set_target_properties(libubpf PROPERTIES
    IMPORTED_LOCATION ${OUTPUT_DIR}/lib/libubpf.a
    INTERFACE_INCLUDE_DIRECTORIES "${UBPF_INCLUDE_DIRS}"
)

add_dependencies(libubpf ubpf)

################ prevail verifier ################
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
add_subdirectory(3p/ebpf-verifier)
set_target_properties(conformance_check PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(bpf_conformance PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(check PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(bpf_conformance_runner PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(run_yaml PROPERTIES EXCLUDE_FROM_ALL TRUE)
set(EBPF_VERIFIER_HEADERS ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src
                          ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/main
                          ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/external)

add_custom_target(copy-verifier-headers ALL DEPENDS ebpfverifier)

add_custom_command(TARGET copy-verifier-headers POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/ 
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/platform.hpp ${OUTPUT_DIR}/inc/
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/config.hpp ${OUTPUT_DIR}/inc/
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/ebpf_base.h ${OUTPUT_DIR}/inc/
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/ebpf_vm_isa.hpp ${OUTPUT_DIR}/inc/
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/helpers.hpp ${OUTPUT_DIR}/inc/
                    COMMAND ${CMAKE_COMMAND} -E copy  ${PROJECT_SOURCE_DIR}/3p/ebpf-verifier/src/spec_type_descriptors.hpp ${OUTPUT_DIR}/inc/  
)

################ Generation of jbpf version info ################                     
add_custom_target(generate_jbpf_lib_version ALL)

add_custom_command(TARGET generate_jbpf_lib_version PRE_BUILD
  COMMAND ${CMAKE_COMMAND}
  -DVMAP=${VERSION_MAP_FILE}
	-DVFILE=${VERSION_FILE}
	-DVHEADER=${VERSION_HEADER}
  -DUSE_NATIVE=${USE_NATIVE}
  -DUSE_JBPF_PERF_OPT=${USE_JBPF_PERF_OPT}
  -DUSE_JBPF_PRINTF_HELPER=${USE_JBPF_PRINTF_HELPER}
  -DJBPF_STATIC=${JBPF_STATIC}
  -DJBPF_THREADS_LARGE=${JBPF_THREADS_LARGE}
  -P ${PROJECT_SOURCE_DIR}/helper_build_files/update_version_info.cmake
)

################ Common files ################
set(JBPF_COMMON_HEADERS ${JBPF_SOURCE_CODE}/common)

add_custom_target(copy-common-headers ALL)

add_custom_command(TARGET copy-common-headers POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_COMMON_HEADERS}/jbpf_defs.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_COMMON_HEADERS}/jbpf_hook_defs_ext.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_COMMON_HEADERS}/jbpf_helper_api_defs_ext.h ${OUTPUT_DIR}/inc/  
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_COMMON_HEADERS}/jbpf_common.h ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_COMMON_HEADERS}/jbpf_common_types.h ${OUTPUT_DIR}/inc/
)

################ Common files ################
set(JBPF_CORE_HEADERS ${JBPF_SOURCE_CODE}/core)

add_custom_target(copy-core-headers ALL)

add_custom_command(TARGET copy-core-headers POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/inc/
  COMMAND ${CMAKE_COMMAND} -E copy  ${JBPF_CORE_HEADERS}/jbpf_helper_utils.h ${OUTPUT_DIR}/inc/
)

################ jbpf logger ################
add_subdirectory(${JBPF_SOURCE_CODE}/logger)

################ jbpf mem_mgmt ################
add_subdirectory(${JBPF_SOURCE_CODE}/mem_mgmt)

################ jbpf io ################
add_subdirectory(${JBPF_SOURCE_CODE}/io)

################ jbpf verifier ################
add_subdirectory(${JBPF_SOURCE_CODE}/verifier)

################ jbpf lcm ################
add_subdirectory(${JBPF_SOURCE_CODE}/lcm)

################ jbpf library ################
add_subdirectory(${JBPF_SOURCE_CODE}/core)

################ jbpf lcm cli ################
add_subdirectory(${JBPF_TOOLS}/lcm_cli)

################ jbpf emulator ################
add_subdirectory(${JBPF_SOURCE_CODE}/emulator)

################ Doxygen ################

find_package(Doxygen)
if (DOXYGEN_FOUND)
  set(DOXYGEN_EXTRACT_STATIC YES)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_HIDE_UNDOC_MEMBERS YES)
  set(DOXYGEN_EXTRACT_PRIVATE NO)
  set(DOXYGEN_EXPAND_AS_DEFINED YES)
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_HAVE_DOT NO)

  set(DOXYGEN_PROJECT_NAME jbpf)
  set(JBPF_DOXYGEN_IN ${PROJECT_SOURCE_DIR}/README.md
                      ${JBPF_SOURCE_CODE}/logger 
                      ${JBPF_SOURCE_CODE}/mem_mgmt
                      ${JBPF_SOURCE_CODE}/io
                      ${JBPF_SOURCE_CODE}/verifier
                      ${JBPF_SOURCE_CODE}/lcm
                      ${JBPF_SOURCE_CODE}/core
  )
  set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
  set(DOXYGEN_OUTPUT_DIRECTORY jbpf_doc)
  set(DOXYGEN_INCLUDE_PATH ${JBPF_SOURCE_CODE}/logger 
                          ${JBPF_SOURCE_CODE}/mem_mgmt
                          ${JBPF_SOURCE_CODE}/io
                          ${JBPF_SOURCE_CODE}/verifier
                          ${JBPF_SOURCE_CODE}/lcm
                          ${JBPF_SOURCE_CODE}/core
                          ${PROJECT_SOURCE_DIR}/docs)
  doxygen_add_docs(jbpf_doc ${JBPF_DOXYGEN_IN})

  add_custom_target(doc
    COMMAND make jbpf_doc)

  add_custom_command(TARGET doc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/jbpf_doc ${OUTPUT_DIR}/docs

    ## the front page of the documentation links to the architecture diagram
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}/docs/html/docs
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/jbpf_oss_architecture.png ${OUTPUT_DIR}/docs/html/docs
  )
else()
  message("Doxygen not found. Please install doxygen package to generate documentation")
endif()

################ Code coverage ################
enable_testing()
# Code coverage will be checked only in Debug mode
if(CMAKE_BUILD_TYPE MATCHES Debug)
  include(CodeCoverage)
  append_coverage_compiler_flags()

  # Print out the compiler flags to verify
  message(STATUS "Coverage CXX Flags: ${CMAKE_CXX_FLAGS}")
  message(STATUS "Coverage C Flags: ${CMAKE_C_FLAGS}")

else()
  message(STATUS "Not in Debug mode, coverage not enabled")
endif()

################ Tests ################
set(TEST_HEADER_FILES ${TESTS_COMMON})
set(JBPF_TESTS
  jbpf::core_lib
  jbpf::io_lib
  jbpf::mem_mgmt_lib
  jbpf::logger_lib
  jbpf::verifier_lib
  jbpf::lcm_ipc_lib
)

if(BUILD_TESTING)  
  add_subdirectory(jbpf_tests)
else()
  message(STATUS "BUILD_TESTING is OFF, skipping jbpf_tests")
endif()
#configure_file(${PROJECT_SOURCE_DIR}/cmake_modules/CTestCustom.cmake ${CMAKE_BINARY_DIR})

################ Basic examples ################
add_subdirectory(examples)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "JBPF_TESTS = ${JBPF_TESTS}")

  set(GCOVR_EXCLUDES
    "--exclude-directories='jbpf_tests'"
    "--exclude-directories='3p'"
    "--exclude='.*libck-jbpf.*'"
    "--exclude='.*jbpf_lookup3.*'"
    "--exclude='.*3p/ebpf-verifier.*'"
    "--exclude='.*tools/lcm_cli/.*'"
    "--exclude='.*src/verifier/jbpf_verifier_cli.cpp'"
  )

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")  

  function(setup_target_for_coverage_gcovr_html NAME EXECUTABLE BASE_DIRECTORY DEPENDENCIES)
    message(STATUS "Setting up ${NAME}_html for coverage gcovr html")
    add_custom_target(${NAME}_html
      COMMAND ${EXECUTABLE}
      COMMAND gcovr ${GCOVR_EXCLUDES}
        --gcov-ignore-parse-errors=negative_hits.warn --html --html-details -o ${NAME}.html --root=${BASE_DIRECTORY}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS ${DEPENDENCIES}
      COMMENT "Generating HTML coverage report with gcovr"
  )
  endfunction()

  function(setup_target_for_coverage_gcovr_xml NAME EXECUTABLE BASE_DIRECTORY DEPENDENCIES)
      message(STATUS "Setting up ${NAME}_html for coverage gcovr xml")
      add_custom_target(${NAME}_xml
        COMMAND ${EXECUTABLE}
        COMMAND gcovr ${GCOVR_EXCLUDES}
          --xml -o ${NAME}.xml --root=${BASE_DIRECTORY}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${DEPENDENCIES}
        COMMENT "Generating XML coverage report with gcovr"
    )
  endfunction()

  setup_target_for_coverage_gcovr_html(
    jbpf_coverage
    ctest
    ${PROJECT_SOURCE_DIR}
    ${JBPF_TESTS}
  )
  setup_target_for_coverage_gcovr_xml(
    jbpf_coverage
    ctest
    ${PROJECT_SOURCE_DIR}
    ${JBPF_TESTS}
  ) 
endif()

if (NOT PROJECT_IS_TOP_LEVEL)
  ######## Export header files for other projects ############
  set(JBPF_IO_HEADER_FILES ${JBPF_IO_HEADER_FILES} PARENT_SCOPE)
  set(JBPF_MEM_MGMT_HEADER_FILES ${JBPF_MEM_MGMT_HEADER_FILES} PARENT_SCOPE)
  set(JBPF_LIB_HEADER_FILES ${JBPF_LIB_HEADER_FILES} PARENT_SCOPE)
  set(JBPF_CK_HEADER_FILES ${JBPF_CK_INCLUDES} PARENT_SCOPE)
  set(JBPF_COMMON_HEADERS ${JBPF_COMMON_HEADERS} PARENT_SCOPE)
  set(JBPF_LCM_IPC_HEADER_FILES ${JBPF_LCM_IPC_HEADER_FILES} PARENT_SCOPE)
endif()
