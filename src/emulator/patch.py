# Copyright (c) Microsoft Corporation. All rights reserved.
import re
import os
import argparse

# Set up argument parsing
parser = argparse.ArgumentParser(description='Add _pack_ attribute to ctypes Structure classes.')
parser.add_argument('output_file', type=str, help='Path to the generated output file (e.g., output.py).')

# Parse the command-line arguments
args = parser.parse_args()
generated_file = args.output_file

# Create a backup of the original file
backup_file = f"{generated_file}.bak"
os.rename(generated_file, backup_file)

# Open the generated output file for reading and writing
with open(backup_file, 'r') as file:
    content = file.readlines()

# Modify the content to add _pack_ = 1
with open(generated_file, 'w') as file:
    for line in content:
        # Look for class definitions that inherit from Structure
        if re.match(r'\s*class\s+\w+\s*\([ctypes\.]{0,1}Structure\)', line):
            # Insert the _pack_ line immediately after the class definition
            file.write(line)
            file.write('    _pack_ = 1\n')  # Add packing
        else:
            file.write(line)

# Optional: Remove the backup file after confirming the modification
# os.remove(backup_file)

print(f"Added _pack_ = 1 to ctypes.Structure classes in {generated_file}.")
