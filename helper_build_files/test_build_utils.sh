#!/bin/bash

source ./build_utils.sh

## Assert $1=$2 print $3
assert_equals() {
    local expected=$1
    local actual=$2
    local message=$3
    if [[ "$expected" != "$actual" ]]; then
        echo "**Test Failed**: $message ("$expected" != "$actual")"
        return 1
    else
        echo "Tess Passed: $message ("$expected" == "$actual")"
        return 0
    fi
}

## Assert $1 contains $2 print $3
assert_contains() {
    local str=$1
    local substr=$2
    local message=$3
    if [[ "$str" =~ "$substr" ]]; then
        echo "Tess Passed: $message (value contains "$substr")"
        return 0
    else
        echo "**Test Failed**: $message (value does not contain "$substr")"
        return 1
    fi
}

## Assert $1 not contains $2 print $3
assert_not_contains() {
    local str=$1
    local substr=$2
    local message=$3
    if [[ "$str" =~ "$substr" ]]; then
        echo "**Test Failed**: $message (value contains "$substr")"
        return 1
    else
        echo "Tess Passed: $message (value does not contain "$substr")"
        return 0
    fi
}


test_flags() {
    local substr="$1"
    get_flags
    if ! assert_contains "$FLAGS" "$substr" "$2"; then
        return 1
    fi
    return 0
}

test_flags_not_contains() {
    local substr="$1"
    get_flags
    if ! assert_not_contains "$FLAGS" "$substr" "$2"; then
        return 1
    fi
}

echo ---------------- Testing the Container Build Configurations and Flags ----------------

### env parameter: JBPF_DEBUG
JBPF_DEBUG=1
if ! test_flags "-DCMAKE_BUILD_TYPE=Debug" "When JBPF_DEBUG=1 flags should contain -DCMAKE_BUILD_TYPE=Debug"; then
    exit 1
fi

JBPF_DEBUG=0
if ! test_flags_not_contains "-DCMAKE_BUILD_TYPE=Debug" "When JBPF_DEBUG=0 flags should not contain -DCMAKE_BUILD_TYPE=Debug"; then
    exit 1
fi

JBPF_DEBUG=
if ! test_flags_not_contains "-DCMAKE_BUILD_TYPE=Debug" "When JBPF_DEBUG is unset flags should not contain -DCMAKE_BUILD_TYPE=Debug"; then
    exit 1
fi

### env parameter: JBPF_STATIC
JBPF_STATIC=1
if ! test_flags "-DJBPF_STATIC=on" "When JBPF_STATIC=1 flags should contain -DJBPF_STATIC=on"; then
    exit 1
fi

JBPF_STATIC=0
if ! test_flags "-DJBPF_STATIC=off" "When JBPF_STATIC=0 flags should contain -DJBPF_STATIC=off"; then
    exit 1
fi

JBPF_STATIC=
if ! test_flags "-DJBPF_STATIC=off" "When JBPF_STATIC is unset flags should contain -DJBPF_STATIC=off"; then
    exit 1
fi

### env parameter: JBPF_THREADS_LARGE
JBPF_THREADS_LARGE=1
if ! test_flags "-DJBPF_THREADS_LARGE=on" "When JBPF_THREADS_LARGE=1 flags should contain -DJBPF_THREADS_LARGE=on"; then
    exit 1
fi

JBPF_THREADS_LARGE=0
if ! test_flags "-DJBPF_THREADS_LARGE=off" "When JBPF_THREADS_LARGE=0 flags should contain -DJBPF_THREADS_LARGE=off"; then
    exit 1
fi

JBPF_THREADS_LARGE=
if ! test_flags "-DJBPF_THREADS_LARGE=off" "When JBPF_THREADS_LARGE is unset flags should contain -DJBPF_THREADS_LARGE=off"; then
    exit 1
fi

### env parameter: USE_NATIVE
USE_NATIVE=1
if ! test_flags "-DUSE_NATIVE=on" "When USE_NATIVE=1 flags should contain -DUSE_NATIVE=on"; then
    exit 1
fi

USE_NATIVE=0
if ! test_flags "-DUSE_NATIVE=off" "When USE_NATIVE=0 flags should contain -DUSE_NATIVE=off"; then
    exit 1
fi

USE_NATIVE=
if ! test_flags "-DUSE_NATIVE=on" "When USE_NATIVE is unset flags should contain -DUSE_NATIVE=on"; then
    exit 1
fi

### env parameter: USE_JBPF_PERF_OPT
USE_JBPF_PERF_OPT=1
if ! test_flags "-DUSE_JBPF_PERF_OPT=on" "When USE_JBPF_PERF_OPT=1 flags should contain -DUSE_JBPF_PERF_OPT=on"; then
    exit 1
fi

USE_JBPF_PERF_OPT=0
if ! test_flags "-DUSE_JBPF_PERF_OPT=off" "When USE_JBPF_PERF_OPT=0 flags should contain -DUSE_JBPF_PERF_OPT=off"; then
    exit 1
fi

USE_JBPF_PERF_OPT=
if ! test_flags "-DUSE_JBPF_PERF_OPT=on" "When USE_JBPF_PERF_OPT is unset flags should contain -DUSE_JBPF_PERF_OPT=on"; then
    exit 1
fi

### env parameter: USE_JBPF_PRINTF_HELPER
USE_JBPF_PRINTF_HELPER=1
if ! test_flags "-DUSE_JBPF_PRINTF_HELPER=on" "When USE_JBPF_PRINTF_HELPER=1 flags should contain -DUSE_JBPF_PRINTF_HELPER=on"; then
    exit 1
fi

USE_JBPF_PRINTF_HELPER=0
if ! test_flags "-DUSE_JBPF_PRINTF_HELPER=off" "When USE_JBPF_PRINTF_HELPER=0 flags should contain -DUSE_JBPF_PRINTF_HELPER=off"; then
    exit 1
fi

USE_JBPF_PRINTF_HELPER=
if ! test_flags "-DUSE_JBPF_PRINTF_HELPER=on" "When USE_JBPF_PRINTF_HELPER is unset flags should contain -DUSE_JBPF_PRINTF_HELPER=on"; then
    exit 1
fi

### env parameter: SANITIZER
SANITIZER=1
if ! test_flags "-DCMAKE_BUILD_TYPE=AddressSanitizer" "When SANITIZER=1 flags should contain -DCMAKE_BUILD_TYPE=AddressSanitizer"; then
    exit 1
fi

SANITIZER=0
if ! test_flags_not_contains "-DCMAKE_BUILD_TYPE=AddressSanitizer" "When SANITIZER=0 flags should not contain -DCMAKE_BUILD_TYPE=AddressSanitizer"; then
    exit 1
fi

SANITIZER=
if ! test_flags_not_contains "-DCMAKE_BUILD_TYPE=AddressSanitizer" "When SANITIZER is unset flags should not contain -DCMAKE_BUILD_TYPE=AddressSanitizer"; then
    exit 1
fi

### env parameter: JBPF_EXPERIMENTAL_FEATURES
JBPF_EXPERIMENTAL_FEATURES=1
if ! test_flags "-DJBPF_EXPERIMENTAL_FEATURES=on" "When JBPF_EXPERIMENTAL_FEATURES=1 flags should contain -DJBPF_EXPERIMENTAL_FEATURES=on"; then
    exit 1
fi

JBPF_EXPERIMENTAL_FEATURES=0
if ! test_flags_not_contains "-DJBPF_EXPERIMENTAL_FEATURES=" "When JBPF_EXPERIMENTAL_FEATURES=0 flags should not contain JBPF_EXPERIMENTAL_FEATURES"; then
    exit 1
fi

JBPF_EXPERIMENTAL_FEATURES=
if ! test_flags_not_contains "-DJBPF_EXPERIMENTAL_FEATURES" "When JBPF_EXPERIMENTAL_FEATURES is unset flags should not contain JBPF_EXPERIMENTAL_FEATURES"; then
    exit 1
fi

### env parameter: CLANG_FORMAT_CHECK
CLANG_FORMAT_CHECK=1
if ! test_flags "-DCLANG_FORMAT_CHECK=on" "When CLANG_FORMAT_CHECK=1 flags should contain -DCLANG_FORMAT_CHECK=on"; then
    exit 1
fi

CLANG_FORMAT_CHECK=0
if ! test_flags_not_contains "-DCLANG_FORMAT_CHECK=" "When CLANG_FORMAT_CHECK=0 flags should not contain CLANG_FORMAT_CHECK"; then
    exit 1
fi

CLANG_FORMAT_CHECK=
if ! test_flags_not_contains "-DCLANG_FORMAT_CHECK" "When CLANG_FORMAT_CHECK is unset flags should not contain CLANG_FORMAT_CHECK"; then
    exit 1
fi

### env parameter: CPP_CHECK
CPP_CHECK=1
if ! test_flags "-DCPP_CHECK=on" "When CPP_CHECK=1 flags should contain -DCPP_CHECK=on"; then
    exit 1
fi

CPP_CHECK=0
if ! test_flags_not_contains "-DCPP_CHECK=" "When CPP_CHECK=0 flags should not contain CPP_CHECK"; then
    exit 1
fi

CPP_CHECK=
if ! test_flags_not_contains "-DCPP_CHECK" "When CPP_CHECK is unset flags should not contain CPP_CHECK"; then
    exit 1
fi

### env parameter: BUILD_TESTING
BUILD_TESTING=1
if ! test_flags "-DBUILD_TESTING=on" "When BUILD_TESTING=1 flags should contain -DBUILD_TESTING=on"; then
    exit 1
fi

BUILD_TESTING=0
if ! test_flags_not_contains "-DBUILD_TESTING=" "When BUILD_TESTING=0 flags should not contain BUILD_TESTING"; then
    exit 1
fi

BUILD_TESTING=
if ! test_flags "-DBUILD_TESTING=on" "When BUILD_TESTING is unset flags should contain -DBUILD_TESTING=on"; then
    exit 1
fi

### env parameter: VERBOSE_LOGGING
VERBOSE_LOGGING=1
if ! test_flags "-DVERBOSE_LOGGING=on" "When VERBOSE_LOGGING=1 flags should contain -DVERBOSE_LOGGING=on"; then
    exit 1
fi

VERBOSE_LOGGING=0
if ! test_flags_not_contains "-DVERBOSE_LOGGING=" "When VERBOSE_LOGGING=0 flags should not contain VERBOSE_LOGGING"; then
    exit 1
fi

VERBOSE_LOGGING=
if ! test_flags_not_contains "-DVERBOSE_LOGGING" "When VERBOSE_LOGGING is unset flags should not contain VERBOSE_LOGGING"; then
    exit 1
fi

echo ---------------- All Passed ----------------
