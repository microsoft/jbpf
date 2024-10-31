#! /bin/bash
source "./helper_build_files/build_utils.sh"

## get the build flags and targets from env parameters
get_flags
echo -e "$OUTPUT"
echo FLAGS=$FLAGS

## set the output directory (inside the docker)
JBPF_OUT_DIR=/jbpf/out

## check clang-format
## please note that different versions of clang-format may have different output
if [[ "$CLANG_FORMAT_CHECK" == "1" ]]; then
    echo "Checking clang-format..."
    cat /jbpf/.clang-format
    DIRS=("src" "jbpf_tests" "examples" "tools")
    cd /jbpf/
    echo The clang-format version is $(clang-format --version)
    for i in "${DIRS[@]}"; do
        echo "Checking clang-format in $i"
        find "$i" -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs clang-format --dry-run --Werror
        if [ $? -ne 0 ]; then
            echo "Error: clang-format check failed."
            exit 1
        fi
    done
fi

## prepare directories
rm -rf $JBPF_OUT_DIR
rm -rf /jbpf/build
mkdir /jbpf/build
cd /jbpf/build

## build
cmake ../ $FLAGS

MAKE_FLAGS=""

if [[ "$VERBOSE" == "1" ]]; then
    echo "Building with VERBOSE=1"
    MAKE_FLAGS="VERBOSE=1"
fi

if ! make -j $MAKE_FLAGS; then
    echo "Error building!"
    exit 1
fi

## if "JBPF_COVERAGE" is set, run make jbpf_coverage
if [[ "$JBPF_COVERAGE" == "1" ]]; then
    make jbpf_coverage_xml
    cp jbpf_coverage.xml $JBPF_OUT_DIR
fi

## if "JBPF_COVERAGE_HTML" is set, run make jbpf_coverage_html
if [[ "$JBPF_COVERAGE_HTML" == "1" ]]; then
    make jbpf_coverage_html
    cp -R jbpf_coverage.html $JBPF_OUT_DIR
fi

## if "DOXYGEN" is set, run make doc
if [[ "$DOXYGEN" == "1" ]]; then
    make doc
fi

if [[ "$RUN_TESTS" == "1" ]]; then
    export JBPF_PATH=/jbpf
    if ! ctest --output-on-failure --output-junit $JBPF_OUT_DIR/jbpf_tests.xml -F; then
        echo "Error running tests!"
        exit 1
    fi
fi

## Test the emulator
if [[ "$RUN_EMULATOR_TESTS" == "1" ]]; then
    export JBPF_PATH=/jbpf
    EMULATOR_TEST_CASES="test_0 test_1 test_2"
    ## TOFIX: disable detect_odr_violation for now
    export ASAN_OPTIONS=detect_odr_violation=0
    export PYTHONMALLOC=malloc
    for test in $EMULATOR_TEST_CASES; do
        if ! /jbpf/out/bin/jbpf_emulator /jbpf/out/emulator/test/ $test; then
            echo "Error running emulator test $test!"
            exit 1
        fi
    done
fi

## Check that all of the libraries we link against are approved, and exit if it fails.
CHECK_LIB_ARGS="--build-dir /jbpf/build --approved-libraries /jbpf/helper_build_files/approved_libraries.txt"
if [[ "$JBPF_STATIC" != "1" ]]; then
    CHECK_LIB_ARGS="$CHECK_LIB_ARGS --so-file /jbpf/out/lib/libjbpf.so"
fi

if ! python3 /jbpf/helper_build_files/check_linked_libraries.py $CHECK_LIB_ARGS; then
    echo "Error: Using unapproved libraries."
    exit 1
fi

## move built files
mkdir -p /jbpf_out_lib
rm -rf /jbpf_out_lib/out
cp -r $JBPF_OUT_DIR /jbpf_out_lib
chmod -R 777 /jbpf_out_lib/out
