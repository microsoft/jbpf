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

using namespace prevail;

static size_t
hash(const RawProgram& raw_prog)
{
    const char* start = reinterpret_cast<const char*>(raw_prog.prog.data());
    const char* end = start + raw_prog.prog.size() * sizeof(EbpfInst);
    return boost::hash_range(start, end);
}

jbpf_verifier_result_t
jbpf_verify(const char* objfile, const char* section, const char* asmfile)
{
    vector<RawProgram> raw_progs;
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
        result.verification_pass = false;
        std::strcpy(result.err_msg, "Could not open ELF file\n");
        std::strcat(result.err_msg, e.what());
        return result;
    }

    if (raw_progs.size() != 1) {
        result.verification_pass = false;
        std::strcpy(result.err_msg, "Could not find the requested section\n");
        return result;
    }

    // Select the last program section.
    RawProgram raw_prog = raw_progs.back();

    // Convert the raw program section to a set of instructions.
    std::variant<InstructionSeq, std::string> prog_or_error = unmarshal(raw_prog);
    if (auto prog = std::get_if<string>(&prog_or_error)) {
        result.verification_pass = false;
        std::strcpy(result.err_msg, "unmarshaling error\n");
        std::strcat(result.err_msg, std::get<string>(prog_or_error).c_str());
        return result;
    }

    auto& inst_seq = std::get<InstructionSeq>(prog_or_error);
    if (!asm_file.empty()) {
        std::ofstream out{asm_file};
        print(inst_seq, out, {});
        print_map_descriptors(thread_local_program_info->map_descriptors, out);
    }

    try {
        const auto verbosity = ebpf_verifier_options.verbosity_opts;
        const Program prog = Program::from_sequence(inst_seq, raw_prog.info, ebpf_verifier_options);
        const auto begin = std::chrono::steady_clock::now();
        auto invariants = analyze(prog);
        const auto end = std::chrono::steady_clock::now();
        const auto seconds = std::chrono::duration<double>(end - begin).count();
        if (verbosity.print_invariants) {
            print_invariants(std::cout, prog, verbosity.simplify, invariants);
        }

        auto report = invariants.check_assertions(prog);
        print_warnings(std::cout, report);
        bool pass = report.verified();

        ebpf_verifier_stats_t verifier_stats;
        if (pass) {
            result.verification_pass = true;
        } else {
            result.verification_pass = false;
        }

        result.max_loop_count = verifier_stats.max_loop_count;
        result.runtime_seconds = seconds;
        return result;

    } catch (UnmarshalError& e) {
        result.verification_pass = false;
        std::strcpy(result.err_msg, "error: ");
        std::strcat(result.err_msg, e.what());
        return result;
    }
}
