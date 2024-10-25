// Copyright (c) Microsoft Corporation. All rights reserved.
#include "CLI11.hpp"

#include "jbpf_verifier.hpp"

int
main(int argc, char** argv)
{
    CLI::App app{"jbpf verifier"};
    const char *asm_file, *section;
    jbpf_verifier_result_t result;

    std::string filename;
    app.add_option("path", filename, "Elf file to analyze")->required()->type_name("FILE");

    std::string desired_section;
    app.add_option("section", desired_section, "Section to analyze")->type_name("SECTION");

    std::string asmfile;
    app.add_option("--asm", asmfile, "Print disassembly to FILE")->type_name("FILE");

    CLI11_PARSE(app, argc, argv);

    if (desired_section.empty()) {
        section = nullptr;
    } else {
        section = desired_section.c_str();
    }

    if (asmfile.empty()) {
        asm_file = nullptr;
    } else {
        asm_file = asmfile.c_str();
    }

    result = jbpf_verify(filename.c_str(), section, asm_file);

    int res;
    if (result.verification_pass) {
        res = 1;
        std::cout << result.verification_pass << "," << result.runtime_seconds << std::endl;
        std::cout << "Program terminates within " << result.max_instruction_count << " instructions\n";
    } else {
        res = 0;
        std::cout << "Failure: " << result.err_msg << std::endl;
    }

    return !res;
}