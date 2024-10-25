#!/bin/bash

for i in "$@"; do
    ## if given parameter is file
    if [ -f "$i" ]; then
        clang-format --dry-run --Werror "$i"
    else
        find . -name "$i" -iname "*.c" -o -iname "*.h" -o -iname "*.cpp" -o -iname "*.hpp" | xargs clang-format --dry-run --Werror
    fi
done
