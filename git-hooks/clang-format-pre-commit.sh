#!/bin/bash
echo "Running clang-format pre-commit hook..."

# Define the clang-format executable
CLANG_FORMAT_BIN=clang-format

# Check if clang-format is installed
if ! command -v ${CLANG_FORMAT_BIN} > /dev/null; then
  echo "Error: clang-format is not installed."
  exit 1
fi

# Get a list of all staged files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cpp|h|hpp)$')

# Exit if there are no files to format
if [ -z "$STAGED_FILES" ]; then
  echo "No C/C++ files staged for commit."
  exit 0
fi

# If the env AUTO_FORMAT is set to 1, format the files
if [ "$AUTO_FORMAT" = "1" ]; then
  echo "Auto-formatting staged files..."
  for FILE in ${STAGED_FILES}; do
    ${CLANG_FORMAT_BIN} --style=file -i "${FILE}"
  done
  exit 0
fi

# Check the formatting of each staged file
FORMAT_ERRORS=0
for FILE in ${STAGED_FILES}; do
  echo "Checking formatting of: ${FILE} ..."
  ${CLANG_FORMAT_BIN} --style=file --dry-run --Werror "${FILE}"
  if [ $? -ne 0 ]; then
    FORMAT_ERRORS=1
  fi
done

# If there were formatting errors, fail the commit
if [ $FORMAT_ERRORS -ne 0 ]; then
  echo "Error: Some files are not formatted correctly. Please run clang-format and stage the changes."
  exit 1
fi

# Exit successfully
exit 0
