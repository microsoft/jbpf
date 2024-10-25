#include <cassert>

#include "jbpf_verifier.hpp"
#include "jbpf_defs.h"
#include "jbpf_helper_api_defs_ext.h"

// Contains the def extensions for the new program types and helper functions
#include "jbpf_verifier_extension_defs.h"

int
main(int argc, char** argv)
{

    std::string full_path;
    jbpf_verifier_result_t result;
    char* jbpf_path = getenv("JBPF_PATH");

    std::string codelet = "/jbpf_tests/test_files/codelets/verifier_extensions/verifier_extensions.o";

    assert(jbpf_path != nullptr);
    full_path = std::string(jbpf_path);
    full_path.append(codelet);

    // Test should fail initially
    result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
    assert(!result.verification_pass);

    // Register the new map type, which is defined in jbpf_verifier_extension_defs.h
    EbpfMapType new_map_type;
    new_map_type.platform_specific_type = CUSTOM_MAP_START_ID;
    new_map_type.name = "NEW_TYPE";
    new_map_type.is_array = false;

    assert(jbpf_verifier_register_map_type(CUSTOM_MAP_START_ID, new_map_type) >= 0);

    // The test should still fail
    result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
    assert(!result.verification_pass);

    // Next we register the new helper function.
    // The function definition can be found in the codelet verifier_extensions.c

    // We first need to define a prototype for the function
    static const struct EbpfHelperPrototype jbpf_new_helper_proto = {
        .name = "new_helper",
        .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer
        .argument_type =
            {
                EBPF_ARGUMENT_TYPE_PTR_TO_MAP, // The first argument is a map pointer
                EBPF_ARGUMENT_TYPE_ANYTHING,   // The next three arguments are generic values
                EBPF_ARGUMENT_TYPE_ANYTHING,
                EBPF_ARGUMENT_TYPE_ANYTHING,
            },
    };

    // Next, we register the prototype and link it to the map type
    jbpf_verifier_register_helper(CUSTOM_HELPER_START_ID, jbpf_new_helper_proto);

    // The test should still fail
    result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
    assert(!result.verification_pass);

    // Finally, we need to register the new program type and context

    // First we identify the context boundaries for the new program type
    /* struct my_new_jbpf_ctx {
        uint64_t data;
        uint64_t data_end;
        uint64_t meta_data;
        uint32_t static_field1;
        uint16_t static_field2;
        uint8_t static_field3;
    }; */

    // Size of the context struct
    // We have three fields of 8 bytes, one of 4 bytes,
    // one of two bytes and one of 1 byte
    constexpr int my_new_jbpf_ctx_regions = 3 * 8 + 4 * 1 + 2 * 1 + 1;

    // Next, we need to create a descriptor with the dynamic pointer fiel offsets (data, data_end, meta_data)
    // data is in offset 0, data_end is 8 bytes later and meta_data 16 bytes from the beginning
    constexpr ebpf_context_descriptor_t my_new_jbpf_ctx_descr = {my_new_jbpf_ctx_regions, 0, 1 * 8, 2 * 8};

    // Next, we define the new program type
    EbpfProgramType new_program_type;
    new_program_type.name = "jbpf_my_prog_type";
    new_program_type.context_descriptor = &my_new_jbpf_ctx_descr;
    new_program_type.section_prefixes = {
        "jbpf_my_prog_type"}; // The name(s) used to define the ELF section,
                              // where the codelet function is stored for this program type
    new_program_type.is_privileged = false;

    jbpf_verifier_register_program_type(CUSTOM_PROGRAM_START_ID, new_program_type);

    // We have now registered all the new verifier extensions.
    // The verification should now pass
    result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
    assert(result.verification_pass);

    return 0;
}
