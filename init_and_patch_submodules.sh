#!/bin/bash

rm -rf 3p/prevail 3p/mimalloc 3p/ck 3p/ubpf
git submodule update --init --recursive

cp patches/ck.patch 3p/ck
cp patches/mimalloc.patch 3p/mimalloc
cp patches/prevail.patch 3p/prevail/
# This is a temporary patch to fix missing BoostConfig.cmake file from AZL3 Boost package
# See: https://github.com/vbpf/prevail/commit/803b647acbfd5985746b664da5d751161dd564ac
cp patches/prevail_cmake.patch 3p/prevail/

cd 3p/ck
git apply ck.patch

cd ../mimalloc
git apply mimalloc.patch

cd ../prevail/
git apply prevail.patch
git apply prevail_cmake.patch
