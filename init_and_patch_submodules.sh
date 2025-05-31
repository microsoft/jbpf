#!/bin/bash

rm -rf 3p/prevail 3p/mimalloc 3p/ck 3p/ubpf
git submodule update --init --recursive

cp patches/ck.patch 3p/ck
cp patches/mimalloc.patch 3p/mimalloc
cp patches/prevail.patch 3p/prevail/

cd 3p/ck
git apply ck.patch

cd ../mimalloc
git apply mimalloc.patch

cd ../prevail/
git apply prevail.patch
