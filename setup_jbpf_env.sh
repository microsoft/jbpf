#!/bin/bash

export JBPF_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd)"
export JBPF_TESTS=$JBPF_PATH/jbpf_tests
export JBPF_OUT_DIR=$JBPF_PATH/out
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$JBPF_PATH/out/lib
