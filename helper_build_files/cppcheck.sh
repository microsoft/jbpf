#!/bin/bash
set -xe

cppcheck \
    --enable=all \
    --suppress=missingInclude \
    --suppress=unusedStructMember \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=variableScope \
    --suppress=unknownMacro \
    --suppress=unreadVariable \
    --suppress=constParameter \
    --suppress=unassignedVariable \
    --suppress=unmatchedSuppression \
    --suppress=passedByValue \
    --suppress=comparePointers \
    --suppress=knownConditionTrueFalse \
    --suppress=constParameterPointer \
    --suppress=checkersReport \
    --suppress=constParameterCallback \
    --suppress=constVariablePointer \
    --error-exitcode=1 \
    --std=c++17 \
    "$@"
