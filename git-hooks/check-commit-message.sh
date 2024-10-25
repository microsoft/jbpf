#!/bin/sh

echo "Running check-commit-message pre-commit hook..."
# Read the commit message from the argument
commit_message_file="$1"

# Extract the first line of the commit message
first_line=$(head -n 1 "$commit_message_file")

# Check the length of the first line
max_length=72
if [ ${#first_line} -gt $max_length ]; then
    echo "Error: The first line of the commit message is too long (more than $max_length characters)."
    echo "Please limit the first line to $max_length characters or less."
    exit 1
fi

# If the first line is within the limit, allow the commit
exit 0
