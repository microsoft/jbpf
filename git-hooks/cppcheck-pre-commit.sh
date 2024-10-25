#!/bin/bash
echo "Running cppcheck pre-commit hook..."

# Define the cppcheck executable
CPPCHECK_BIN=cppcheck

# disable cppcheck flag
if [ "$DISABLE_CPPCHECK" = "1" ]; then
  echo "cppcheck pre-commit hook disabled."
  exit 0
fi

# Check if cppcheck is installed
if ! command -v ${CPPCHECK_BIN} > /dev/null; then
  echo "Error: cppcheck is not installed."
  exit 0
fi

# Get a list of all staged files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cpp|h|hpp)$')

# Exit if there are no files to check
if [ -z "$STAGED_FILES" ]; then
  echo "No C/C++ files staged for commit."
  exit 0
fi

# Run cppcheck on each staged file
CHECK_ERRORS=0
for FILE in ${STAGED_FILES}; do
  echo "Running cppcheck on: ${FILE} ..."
  $JBPF_PATH/helper_build_files/cppcheck.sh "${FILE}"
  if [ $? -ne 0 ]; then
    CHECK_ERRORS=1
  fi
done

# If cppcheck found any issues, fail the commit
if [ $CHECK_ERRORS -ne 0 ]; then
  echo "Error: cppcheck found issues. Please fix them before committing."
  exit 1
fi

# Exit successfully
exit 0
