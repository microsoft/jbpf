/*
 * The purpose of this test is to verify that all map configurations can be supported.
 *
 * This test does the following:
 * 1. It attempts to register 6 different map types with the verifier.  These are:-
 *     - value_type = EbpfMapValueType::ANY, is_array = false
 *     - value_type = EbpfMapValueType::ANY, is_array = true
 *     - value_type = EbpfMapValueType::MAP, is_array = false
 *     - value_type = EbpfMapValueType::MAP, is_array = true
 *     - value_type = EbpfMapValueType::PROGRAM, is_array = false
 *     - value_type = EbpfMapValueType::PROGRAM, is_array = true
 * 2.  It asserts that the first 4 are registered successfully and the last 2 are not.
 * 3.  It registers a helper function "helper_func_1" with the verifier.  This helper function takes a pointer to a map.
 * 4.  It verifies that codelet "helper_funcs/all_maps.o" passes verification.  This codelet calls the helper function
 for each of the first 4 maps defintions above.
 *
 */

#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
// #include "jbpf_verifier_extension_defs.h"
#include "jbpf_utils.h"
// #include "jbpf_helper_api_defs_ext.h"
// #include "jbpf_verifier.hpp"
// #include "jbpf_verifier_extension_defs.h"

#include "jbpf_verifier.hpp"
#include "jbpf_defs.h"
#include "jbpf_helper_api_defs_ext.h"

// Contains the def extensions for the new program types and helper functions
#include "jbpf_verifier_extension_defs.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_ITERATIONS 5

struct packet p = {0};

#define NUM_HELPERS 3
int helper_func_num_calls[NUM_HELPERS] = {0};

sem_t sem;

static uint16_t processed = 0;

static jbpf_io_stream_id_t stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

#define NUM_EXPECTED_OUTPUTS (4)

/* return:
        EBPF_RETURN_TYPE_INTEGER
   args:
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
        EBPF_ARGUMENT_TYPE_ANYTHING
*/
static int
helper_func_1(void* ptr_to_map_arg, uint64_t anything_arg1)
{
    printf("helper_func_1: %p \n", ptr_to_map_arg);
    printf("helper_func_1: %lu \n", anything_arg1);
    return 1111;
}

int
main(int argc, char** argv)
{
    const char* jbpf_path = getenv("JBPF_PATH");

    int map_id = CUSTOM_MAP_START_ID;

    // map 1
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map1";
        new_map_type.is_array = false;
        new_map_type.value_type = EbpfMapValueType::ANY;
        printf("Registering new_map1\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) >= 0);
        map_id++;
    }

    // map 2
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map2";
        new_map_type.is_array = true;
        new_map_type.value_type = EbpfMapValueType::ANY;
        printf("Registering new_map2\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) >= 0);
        map_id++;
    }

    // map 3
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map3";
        new_map_type.is_array = true;
        new_map_type.value_type = EbpfMapValueType::MAP;
        printf("Registering new_map3\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) >= 0);
        map_id++;
    }

    // map 4
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map4";
        new_map_type.is_array = true;
        new_map_type.value_type = EbpfMapValueType::MAP;
        printf("Registering new_map4\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) >= 0);
        map_id++;
    }

    // map 5
    // EbpfMapValueType::PROGRAM is not currently supported so this should fail
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map5";
        new_map_type.is_array = false;
        new_map_type.value_type = EbpfMapValueType::PROGRAM;
        printf("Registering new_map5\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) < 0);
    }

    // map 6
    // EbpfMapValueType::PROGRAM is not currently supported so this should fail
    {
        EbpfMapType new_map_type;
        new_map_type.platform_specific_type = map_id;
        new_map_type.name = "new_map6";
        new_map_type.is_array = true;
        new_map_type.value_type = EbpfMapValueType::PROGRAM;
        printf("Registering new_map6\n");
        assert(jbpf_verifier_register_map_type(map_id, new_map_type) < 0);
    }

    // helper func
    {
        int helper_id = CUSTOM_HELPER_START_ID;

        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_1};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_1_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer
            .argument_type = {EBPF_ARGUMENT_TYPE_PTR_TO_MAP, EBPF_ARGUMENT_TYPE_ANYTHING},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_1\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);
    }

    assert(jbpf_path != NULL);
    char codelet_path[JBPF_PATH_LEN];
    snprintf(codelet_path, JBPF_PATH_LEN, "%s/jbpf_tests/test_files/codelets/helper_funcs/all_maps.o", jbpf_path);

    // The test should still fail
    auto result = jbpf_verify(codelet_path, nullptr, nullptr);
    printf("result.verification_pass: %d\n", result.verification_pass);
    assert(result.verification_pass);

    // We reset the helper functions. The codelet should fail to load again
    jbpf_reset_helper_functions();

    printf("Test completed successfully\n");
    return 0;
}
