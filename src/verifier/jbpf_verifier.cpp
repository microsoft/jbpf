// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright (c) Prevail Verifier contributors.
// SPDX-License-Identifier: MIT
#include <iostream>
#include <vector>

#include <boost/functional/hash.hpp>

#include "ebpf_verifier.hpp"
#include "memsize_linux.hpp"
#include "jbpf_platform.hpp"
#include "jbpf_verifier.hpp"

using std::string;
using std::vector;

static size_t
hash(const raw_program& raw_prog)
{
    char* start = (char*)raw_prog.prog.data();
    char* end = start + (raw_prog.prog.size() * sizeof(ebpf_inst));
    return boost::hash_range(start, end);
}

jbpf_verifier_result_t
jbpf_verify(const char* objfile, const char* section, const char* asmfile)
{
    vector<raw_program> raw_progs;
    std::string desired_section, asm_file;
    ebpf_verifier_options_t ebpf_verifier_options;
    jbpf_verifier_result_t result = {0};

    if (section != nullptr) {
        desired_section = section;
    }

    if (asmfile != nullptr) {
        asm_file = asmfile;
    }

    // Set jbpf default options
    ebpf_verifier_options.cfg_opts.check_for_termination = true;
    ebpf_verifier_options.verbosity_opts.print_failures = true;
    ebpf_verifier_options.allow_division_by_zero = true;

    const ebpf_platform_t platform = g_ebpf_platform_jbpf;

    try {
        raw_progs = read_elf(objfile, desired_section, ebpf_verifier_options, &platform);
    } catch (std::runtime_error& e) {
        std::cout << e.what() << std::endl;
        result.verification_pass = false;
        std::strcpy(result.err_msg, "Could not open ELF file\n");
        return result;
    }

    if (raw_progs.size() != 1) {
        result.verification_pass = false;
        std::strcpy(result.err_msg, "Could not find the requested section\n");
        return result;
    }

    // Select the last program section.
    raw_program raw_prog = raw_progs.back();

    // Convert the raw program section to a set of instructions.
    std::variant<InstructionSeq, std::string> prog_or_error = unmarshal(raw_prog);
    if (auto prog = std::get_if<string>(&prog_or_error)) {
        std::cout << "unmarshaling error at " << *prog << "\n";
        return result;
    }

    auto& prog = std::get<InstructionSeq>(prog_or_error);
    if (!asm_file.empty()) {
        std::ofstream out{asm_file};
        print(prog, out, {});
        print_map_descriptors(thread_local_program_info->map_descriptors, out);
    }

    // Convert the instruction sequence to a control-flow graph.
    try {
        const auto verbosity = ebpf_verifier_options.verbosity_opts;
        const cfg_t cfg = prepare_cfg(prog, raw_prog.info, ebpf_verifier_options.cfg_opts);

        const auto begin = std::chrono::steady_clock::now();
        auto invariants = analyze(cfg);
        const auto end = std::chrono::steady_clock::now();
        const auto seconds = std::chrono::duration<double>(end - begin).count();

        bool pass;
        if (verbosity.print_failures) {
            auto report = invariants.check_assertions(cfg);
            print_warnings(std::cout, report);
            pass = report.verified();
        } else {
            pass = invariants.verified(cfg);
        }
        if (pass) {
            result.verification_pass = true;
        } else {
            result.verification_pass = false;
        }

        result.max_loop_count = invariants.max_loop_count();
        result.runtime_seconds = seconds;

        return result;
    } catch (UnmarshalError& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return result;
    }
}
