#!/usr/bin/env python3 Copyright (c) Microsoft Corporation. All rights reserved.
#
# jbpf depends on (via static or dynamic linking) various Linux libraries.
# Component Governance does not supporting raising legal alerts for these (see
# https://docs.opensource.microsoft.com/tools/cg/component-detection/feature-coverage/),
# so we have to follow
# https://docs.opensource.microsoft.com/tools/cg/legal/unsupported-components/
# to get CELA's review of them.
#
# This script makes that easier by figuring out what libraries we're actually
# linking jbpf with and checking those against the approved list. We call it in
# the pipeline and fail the build if a new one is added without approval, to
# prompt us to get CELA approval.
import os
import subprocess
import sys
import argparse

# Extract all the ".a" files from a link.txt file
def extract_libs_from_link_txt(link_txt):
    libs = []
    with open(link_txt, 'r') as file:
        link_txt = file.read()
    for lib in link_txt.split():
        if lib.endswith('.a'):
            libs.append(lib)
    return libs


# Extract all the shared libraries the specified library says it depends on.
def check_linked_libraries(lib_path):
    try:
        output = subprocess.check_output(['objdump', '-p', lib_path])
    except subprocess.CalledProcessError as e:
        print('Error: objdump failed with exit code {}'.format(e.returncode))
        raise e

    output = output.decode('utf-8')
    lines = output.split('\n')
    needed = []
    for line in lines:
        if 'NEEDED' in line:
            # Just take up to the first "." and append ".so", to handle cases
            # where there are trailing digits e.g. libpthread.so.5 ->
            # libpthread.so. These digits are often different between distros
            # and we don't want to have to keep updating the approved list for
            # them.
            library_name = line.split()[1]
            needed.append(library_name.split('.')[0] + '.so')

    return needed


# Compare a set of libraries against a file of approved libraries.
def compare_with_approved(found_libraries, approved_libraries_file):
    # Read the approved libraries from the file
    with open(approved_libraries_file, 'r') as file:
        approved_libraries = {line.strip() for line in file}

    unapproved_libraries = found_libraries - approved_libraries

    if not unapproved_libraries:
        print("All linked libraries are approved.")
    else:
        print("The following linked libraries are not approved:")
        for lib in unapproved_libraries:
            print(lib)
        raise Exception("""
    Unapproved linked libraries found. To resolve this issue:
        1.  identify which package provides the unapproved library (using e.g.
            `rpm -qf <file>` in an RPM-based build container)
        2.  identify its license
        3.  submit an unsupported components review request to CELA via the
            process documented at
            https://docs.opensource.microsoft.com/tools/cg/legal/unsupported-components/
        4.  once the review is complete, update the approved libraries list in
            helper_build_files/approved_libraries.txt

    Note that we want CELA's advice/approval on these libraries, even though we
    aren't distributing them alongside jbpf (as they will typically be
    installed by the vendor who is integrating jbpf into their system), because
    we want to be reasonably confident that the licensing of these libraries
    will not prevent those vendors from distributing jbpf along with the libraries.
                        """)


def main():
    parser = argparse.ArgumentParser(description="Process some files.")
    parser.add_argument('--so-file', type=str, help='Path to the .so file')
    parser.add_argument('--build-dir', type=str, help='Path to the build directory to search for link.txt files')
    parser.add_argument('--approved-libraries', type=str, help='Path to the approved libraries list file')

    args = parser.parse_args()
    libraries = []

    if args.so_file:
        libraries.extend(check_linked_libraries(args.so_file))

    if args.build_dir:
        for root, _, files in os.walk(args.build_dir):
            for file in files:
                if file == 'link.txt':
                    link_txt = os.path.join(root, file)
                    libraries.extend(extract_libs_from_link_txt(link_txt))

    # Convert to a set to remove duplicates and prepare for comparison with the
    # set of approved libraries.
    library_set = set(os.path.basename(lib) for lib in libraries)

    if args.approved_libraries:
        compare_with_approved(library_set, args.approved_libraries)
    else:
        # If we aren't provided with an approved list, just print the libraries
        # we found.
        for lib in library_set:
            print(lib)

if __name__ == "__main__":
    main()
