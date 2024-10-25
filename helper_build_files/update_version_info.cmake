function(update_version_info version_file version_map version_header use_native use_jbpf_perf_opt use_jbpf_printf_helper jbpf_static jbpf_threads_large)
  file(READ ${version_file} ver)
  string(REGEX MATCH "VERSION_MAJOR ([0-9]*)" _ ${ver})
  set(ver_major ${CMAKE_MATCH_1})
  string(REGEX MATCH "VERSION_MINOR ([0-9]*)" _ ${ver})
  set(ver_minor ${CMAKE_MATCH_1})
  string(REGEX MATCH "VERSION_PATCH ([0-9]*)" _ ${ver})
  set(ver_patch ${CMAKE_MATCH_1})
  string(REGEX MATCH "C_API_VERSION ([0-9]*)" _ ${ver})
  set(c_api_ver ${CMAKE_MATCH_1})
  message("jbpf version: ${ver_major}.${ver_minor}.${ver_patch}+${c_api_ver}")

  # Update the version information of the map and header files
  file(READ "${version_map}.in" ver_file)
  string(REGEX REPLACE "^jbpf_ver_[0-9.]+ {" "jbpf_ver_${c_api_ver} {" ver_file_res "${ver_file}")
  file(WRITE ${version_map} "${ver_file_res}")

  set(JBPF_VERSION_MAJOR "${ver_major}")
  set(JBPF_VERSION_MINOR "${ver_minor}")
  set(JBPF_VERSION_PATCH "${ver_patch}")
  set(C_API_VERSION "${c_api_ver}")

  # Build parameters
  message("USE_NATIVE = ${use_native}")
  message("USE_JBPF_PERF_OPT = ${use_jbpf_perf_opt}")
  message("USE_JBPF_PRINTF_HELPER = ${use_jbpf_printf_helper}")
  message("JBPF_STATIC = ${jbpf_static}")
  message("JBPF_THREADS_LARGE = ${jbpf_threads_large}")
  
  set(USE_NATIVE "${use_native}")
  set(USE_JBPF_PERF_OPT "${use_jbpf_perf_opt}")
  set(USE_JBPF_PRINTF_HELPER "${use_jbpf_printf_helper}")
  set(JBPF_STATIC "${jbpf_static}")
  set(JBPF_THREADS_LARGE "${jbpf_threads_large}")

  # Update the commit and branch that is used to build jbpf
  execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT_VARIABLE JBPF_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

  execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	OUTPUT_VARIABLE JBPF_COMMIT_BRANCH
  	OUTPUT_STRIP_TRAILING_WHITESPACE
	)
  configure_file("${version_header}.in" "${version_header}")

endfunction()

update_version_info(${VFILE} ${VMAP} ${VHEADER} ${USE_NATIVE} ${USE_JBPF_PERF_OPT} ${USE_JBPF_PRINTF_HELPER} ${JBPF_STATIC} ${JBPF_THREADS_LARGE} ${JBPF_EXPERIMENTAL_FEATURES})
