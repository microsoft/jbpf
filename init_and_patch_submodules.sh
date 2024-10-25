#!/bin/bash

rm -rf 3p/ebpf-verifier 3p/mimalloc 3p/ck 3p/ubpf
git submodule update --init --recursive

cp patches/ck.patch 3p/ck
cp patches/mimalloc.patch 3p/mimalloc
cp patches/ebpf_verifier.patch 3p/ebpf-verifier/

cd 3p/ck
git apply ck.patch

cd ../mimalloc
git apply mimalloc.patch

cd ../ebpf-verifier/
git apply ebpf_verifier.patch
