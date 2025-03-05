#!/bin/bash
## provide bash functions to help build docker containers

## get cmake build flags
### env parameter: JBPF_DEBUG
### env parameter: JBPF_STATIC
### env parameter: USE_NATIVE
### env parameter: USE_JBPF_PERF_OPT
### env parameter: USE_JBPF_PRINTF_HELPER
### env parameter: SANITIZER
### env parameter: JBPF_THREADS_LARGE
### env parameter: JBPF_EXPERIMENTAL_FEATURES
### env parameter: CLANG_FORMAT_CHECK
### env parameter: CPP_CHECK
### env parameter: BUILD_TESTING
get_flags() {
    FLAGS=""
    OUTPUT=""
    if [[ "$JBPF_DEBUG" == "1" ]]; then
        OUTPUT="$OUTPUT Building jbpf with debug symbols\n"
        FLAGS="$FLAGS -DCMAKE_BUILD_TYPE=Debug"
    fi
    if [[ "$JBPF_STATIC" == "1" ]]; then
        OUTPUT="$OUTPUT Building jbpf as a static library\n"
        FLAGS="$FLAGS -DJBPF_STATIC=on"
    else
        OUTPUT="$OUTPUT Building jbpf as a dynamic library\n"
        FLAGS="$FLAGS -DJBPF_STATIC=off"
    fi
    if [[ "$USE_NATIVE" == "1" || "$USE_NATIVE" == "" ]]; then
        OUTPUT="$OUTPUT Enabling flag -march=native\n"
        FLAGS="$FLAGS -DUSE_NATIVE=on"
    else
        OUTPUT="$OUTPUT Disabling flag -march=native\n"
        FLAGS="$FLAGS -DUSE_NATIVE=off"
    fi
    if [[ "$USE_JBPF_PERF_OPT" == "1" || "$USE_JBPF_PERF_OPT" == "" ]]; then
        OUTPUT="$OUTPUT Enabling performance optimizations\n"
        FLAGS="$FLAGS -DUSE_JBPF_PERF_OPT=on"
    else
        OUTPUT="$OUTPUT Disabling performance optimizations\n"
        FLAGS="$FLAGS -DUSE_JBPF_PERF_OPT=off"
    fi
    if [[ "$USE_JBPF_PRINTF_HELPER" == "1" || "$USE_JBPF_PRINTF_HELPER" == "" ]]; then
        OUTPUT="$OUTPUT Enable use of jbpf_printf_debug()\n"
        FLAGS="$FLAGS -DUSE_JBPF_PRINTF_HELPER=on"
    else
        OUTPUT="$OUTPUT Disable use of jbpf_printf_debug()\n"
        FLAGS="$FLAGS -DUSE_JBPF_PRINTF_HELPER=off"
    fi
    if [[ "$SANITIZER" == "1" ]]; then
        OUTPUT="$OUTPUT Building with Address Sanitizer\n"
        FLAGS="$FLAGS -DUSE_NATIVE=OFF -DCMAKE_BUILD_TYPE=AddressSanitizer"
    fi
    if [[ "$JBPF_THREADS_LARGE" == "1" ]]; then
        OUTPUT="$OUTPUT Building with large number of jbpf threads\n"
        FLAGS="$FLAGS -DJBPF_THREADS_LARGE=on"
    else
        OUTPUT="$OUTPUT Disabling large number of jbpf threads\n"
        FLAGS="$FLAGS -DJBPF_THREADS_LARGE=off"
    fi
    if [[ "$JBPF_EXPERIMENTAL_FEATURES" == "1" ]]; then
        OUTPUT="$OUTPUT Building with experimental features enabled\n"
        FLAGS="$FLAGS -DJBPF_EXPERIMENTAL_FEATURES=on"
    else 
        OUTPUT="$OUTPUT Building with experimental features unset\n"
    fi
    if [[ "$CLANG_FORMAT_CHECK" == "1" ]]; then
        OUTPUT="$OUTPUT Checking with clang-format\n"
        FLAGS="$FLAGS -DCLANG_FORMAT_CHECK=on"
    else 
        OUTPUT="$OUTPUT Skipping clang-format check\n"
    fi
    if [[ "$CPP_CHECK" == "1" ]]; then
        OUTPUT="$OUTPUT Checking with cppcheck\n"
        FLAGS="$FLAGS -DCPP_CHECK=on"
    else
        OUTPUT="$OUTPUT Skipping cppcheck\n"
    fi
    if [[ "$BUILD_TESTING" == "1" || "$BUILD_TESTING" == "" ]]; then
        OUTPUT="$OUTPUT Build with tests\n"
        FLAGS="$FLAGS -DBUILD_TESTING=on"
    else
        OUTPUT="$OUTPUT Skipping building tests\n"
    fi
}
