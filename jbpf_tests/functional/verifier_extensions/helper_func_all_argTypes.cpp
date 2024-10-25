/*
 * The purpose of this test is to verify that the jbpf verifier extension APIs work correctly.
 *
 * This test does the following:
 * 1. It creates 5 helper functions which, between them, use all of the supported helper function return types and arg
 * types.
 * 2. These helper functions are registered with the jbpf verifier.
 * 3. It configures a codeletSet which includes the codelet "helper_funcs/helper_funcs_all_argTypes.o".  This codelet
 * calls all the helper functions.
 * 4. It asserts that the codelet passes verification.
 * 5. It calls the hook.
 * 6. It asserts that the helper functions were called the expected number of times, and that the data received in
 * the output channel is also correct.
 * 7.  It unloads the codeletset and resets the helper functions.
 *
 * NOTE: EBPF_ARGUMENT_TYPE_PTR_TO_MAP_OF_PROGRAMS is not supported in the current implementation of this test.
 */
#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_utils.h"
#include "jbpf_verifier.hpp"
#include "jbpf_defs.h"
#include "jbpf_helper_api_defs_ext.h"

// Contains the def extensions for the new program types and helper functions
#include "jbpf_verifier_extension_defs.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

#define NUM_ITERATIONS 5

#define META_DATA (1)
#define STATIC_FIELD1 (2)
#define STATIC_FIELD2 (3)
#define STATIC_FIELD3 (4)
#define COUNTER_A (2222)
#define HELPER_FUNC_1_RETURN (1111)
#define HELPER_FUNC_3_RETURN (3333)
#define HELPER_FUNC_5_RETURN (5555)

struct packet p = {0};

#define NUM_HELPERS 5
int helper_func_num_calls[NUM_HELPERS] = {0};

sem_t sem;

static uint16_t processed = 0;

static jbpf_io_stream_id_t stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

#define NUM_EXPECTED_OUTPUTS (4)

/* return:
        EBPF_RETURN_TYPE_INTEGER
   args:
        EBPF_ARGUMENT_TYPE_DONTCARE,
        EBPF_ARGUMENT_TYPE_ANYTHING,
        EBPF_ARGUMENT_TYPE_CONST_SIZE,
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
        EBPF_ARGUMENT_TYPE_PTR_TO_CTX
*/
static int
helper_func_1(
    uint64_t dont_care_arg,
    uint64_t anything_arg,
    uint64_t const_size_arg,
    uint64_t const_size_or_zero_arg,
    void* ptr_to_ctx_arg)
{
    printf("helper_func_1: %lu \n", dont_care_arg);
    printf("helper_func_1: %lu \n", anything_arg);
    printf("helper_func_1: %lu \n", const_size_arg);
    printf("helper_func_1: %lu \n", const_size_or_zero_arg);
    printf("helper_func_1: %p \n", ptr_to_ctx_arg);
    helper_func_num_calls[0]++;
    return HELPER_FUNC_1_RETURN;
}

/* return:
        EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL
   args:
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_OF_PROGRAMS
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY
        EBPF_ARGUMENT_TYPE_PTR_TO_MAP_VALUE
*/
static void*
helper_func_2(
    void* ptr_to_map_arg, void* ptr_to_map_of_programs_arg, void* ptr_to_map_key_arg, void* ptr_to_map_value_arg)
{
    printf("helper_func_2: %p \n", ptr_to_map_arg);
    printf("helper_func_2: %p \n", ptr_to_map_of_programs_arg);
    printf("helper_func_2: %p \n", ptr_to_map_key_arg);
    printf("helper_func_2: %p \n", ptr_to_map_value_arg);
    helper_func_num_calls[1]++;
    return ptr_to_map_value_arg;
}

/* return:
        EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED
   args:
        EBPF_ARGUMENT_TYPE_ANYTHING
        EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
        EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM_OR_NULL
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
*/
static int
helper_func_3(
    uint64_t anything_arg,
    void* ptr_to_readable_mem_arg,
    uint64_t readable_mem_size_or_zero_arg,
    void* ptr_to_readable_mem_or_null_arg,
    uint64_t readable_mem_or_null_size_or_zero_arg)
{
    printf("helper_func_3: %lu \n", anything_arg);
    printf("helper_func_3: %p \n", ptr_to_readable_mem_arg);
    printf("helper_func_3: %lu \n", readable_mem_size_or_zero_arg);
    printf("helper_func_3: %p \n", ptr_to_readable_mem_or_null_arg);
    printf("helper_func_3: %lu \n", readable_mem_or_null_size_or_zero_arg);
    helper_func_num_calls[2]++;
    return HELPER_FUNC_3_RETURN;
}

static void
helper_func_4(
    uint64_t anything_arg,
    void* ptr_to_readable_mem_arg,
    uint64_t readable_mem_size_or_zero_arg,
    void* ptr_to_readable_mem_or_null_arg,
    uint64_t readable_mem_or_null_size_or_zero_arg)
{
    printf("helper_func_4: %lu \n", anything_arg);
    printf("helper_func_4: %p \n", ptr_to_readable_mem_arg);
    printf("helper_func_4: %lu \n", readable_mem_size_or_zero_arg);
    printf("helper_func_4: %p \n", ptr_to_readable_mem_or_null_arg);
    printf("helper_func_4: %lu \n", readable_mem_or_null_size_or_zero_arg);
    helper_func_num_calls[3]++;
}

/* return:
        EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED
   args:
        EBPF_ARGUMENT_TYPE_ANYTHING,
        EBPF_ARGUMENT_TYPE_PTR_TO_WRITABLE_MEM,
        EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO
*/
static int
helper_func_5(uint64_t anything_arg, void* ptr_to_writable_mem_arg, uint64_t const_size_or_zero_arg)
{
    printf("helper_func_5: %lu \n", anything_arg);
    printf("helper_func_5: %p \n", ptr_to_writable_mem_arg);
    printf("helper_func_5: %lu \n", const_size_or_zero_arg);
    helper_func_num_calls[4]++;
    return HELPER_FUNC_5_RETURN;
}

static void
io_channel_check_output(jbpf_io_stream_id_t* streamId, void** bufs, int num_bufs, void* ctx)
{
    int* output;

    for (int i = 0; i < num_bufs; i++) {

        // printf("stream = ");
        // for (int j = 0; j < JBPF_STREAM_ID_LEN; j++) {
        //     printf("%02x", streamId->id[j]);
        // }
        // printf("\n");

        assert(memcmp(streamId, &stream_id, sizeof(jbpf_io_stream_id_t)) == 0);

        output = (int*)bufs[i];
        // printf("output=%d\n", *output);
        assert(
            *output == HELPER_FUNC_1_RETURN || *output == (COUNTER_A + STATIC_FIELD1) ||
            *output == (HELPER_FUNC_3_RETURN + STATIC_FIELD2) || *output == (HELPER_FUNC_5_RETURN + STATIC_FIELD3));

        processed++;
    }

    // printf("processed = %d\n", processed);

    if (processed == NUM_EXPECTED_OUTPUTS) {
        sem_post(&sem);
    }
}

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req = {0};
    struct jbpf_codeletset_load_req* codset = &codeletset_req;
    struct jbpf_codeletset_unload_req codeletset_unload_req = {0};
    struct jbpf_codeletset_unload_req* codset_ul = &codeletset_unload_req;

    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);

    sem_init(&sem, 0, 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    int helper_id = CUSTOM_HELPER_START_ID;

    // helper func 1
    {
        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_1};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_1_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer
            .argument_type =
                {EBPF_ARGUMENT_TYPE_DONTCARE,
                 EBPF_ARGUMENT_TYPE_ANYTHING,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
                 EBPF_ARGUMENT_TYPE_PTR_TO_CTX},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_1\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);

        helper_id++;
    }

    // helper_func_2
    {
        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_2};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_2_proto",
            .return_type =
                EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL, // The function returns a pointer to map value or null
            .argument_type =
                {EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
                 // TODO: resolve the following type
                 EBPF_ARGUMENT_TYPE_DONTCARE, // EBPF_ARGUMENT_TYPE_PTR_TO_MAP_OF_PROGRAMS,
                 EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY,
                 EBPF_ARGUMENT_TYPE_PTR_TO_MAP_VALUE},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_2\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);

        helper_id++;
    }

    // helper_func_3
    {
        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_3};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_3_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED, // The function returns an integer or no
                                                                             // return if succeed
            .argument_type =
                {EBPF_ARGUMENT_TYPE_ANYTHING,
                 EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
                 EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM_OR_NULL,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_3\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);

        helper_id++;
    }

    // helper_func_4
    {
        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_4};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_4_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED, // The function returns an integer or no
                                                                             // return if succeed
            .argument_type =
                {EBPF_ARGUMENT_TYPE_ANYTHING,
                 EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
                 EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM_OR_NULL,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_4\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);

        helper_id++;
    }

    // helper_func_5
    {
        jbpf_helper_func_def_t helper_func = {"new_helper_func", helper_id, (jbpf_helper_func_t)helper_func_5};
        helper_func.reloc_id = helper_id;
        assert(jbpf_register_helper_function(helper_func) == 0); // new registration

        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_5_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED, // The function returns an integer or no
                                                                             // return if succeed
            .argument_type =
                {EBPF_ARGUMENT_TYPE_ANYTHING,
                 EBPF_ARGUMENT_TYPE_PTR_TO_WRITABLE_MEM,
                 EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO},
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering helper_func_5\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);

        helper_id++;
    }

    // helper_func 'return' not supported
    {
        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_not_supported_proto",
            .return_type = EBPF_RETURN_TYPE_UNSUPPORTED, // The function returns an integer or no return if succeed
            .argument_type =
                {
                    EBPF_ARGUMENT_TYPE_ANYTHING,
                },
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering : helper_func 'return' not supported\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);
    }

    // helper_func 'arg' not supported
    {
        // prototype for the function
        static const struct EbpfHelperPrototype helper_proto = {
            .name = "helper_func_not_supported_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer or no return if succeed
            .argument_type =
                {
                    EBPF_ARGUMENT_TYPE_UNSUPPORTED,
                },
        };

        // Next, we register the prototype and link it to the map type
        printf("Registering : helper_func 'arg' not supported\n");
        jbpf_verifier_register_helper(helper_id, helper_proto);
    }

    // The name of the codeletset
    strcpy(codset->codeletset_id.name, "helper_funcs_all_argTypes_codeletset");

    // We have only one codelet in this codeletset
    codset->num_codelet_descriptors = 1;

    jbpf_codelet_descriptor_s* cod = &codset->codelet_descriptor[0];

    // The codelet has just one output channel and no shared maps
    cod->num_in_io_channel = 0;
    cod->num_linked_maps = 0;
    cod->num_out_io_channel = 1;

    jbpf_io_channel_desc_s* ch = &cod->out_io_channel[0];

    strcpy(ch->name, "output_map");
    memcpy(&ch->stream_id, &stream_id, JBPF_STREAM_ID_LEN);
    ch->has_serde = false;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        cod->codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/helper_funcs/helper_funcs_all_argTypes.o",
        jbpf_path);
    strcpy(cod->codelet_name, "helper_funcs_max_output");
    strcpy(cod->hook_name, "test7");

    // The codelet should fail verification at this point
    {
        auto result = jbpf_verify(cod->codelet_path, nullptr, nullptr);
        printf("result.verification_pass: %d\n", result.verification_pass);
        assert(!result.verification_pass);
    }

    // Next add the jbpf_my_prog_type

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

    // The codelet should now pass verification
    {
        auto result = jbpf_verify(cod->codelet_path, nullptr, nullptr);
        printf("result.verification_pass: %d\n", result.verification_pass);
        assert(result.verification_pass);
    }

    // Load the codeletset. Loading should fail
    assert(jbpf_codeletset_load(codset, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call hook
    my_new_jbpf_ctx ctx = {0};
    ctx.data = (uint64_t)&p;
    p.counter_a = COUNTER_A;
    uint64_t meta_data = META_DATA;
    uint32_t static_field1 = STATIC_FIELD1;
    uint16_t static_field2 = STATIC_FIELD2;
    uint8_t static_field3 = STATIC_FIELD3;

    hook_test7(&p, meta_data, static_field1, static_field2, static_field3);
    // check helper functions were called the expected number of times
    for (int i = 0; i < NUM_HELPERS; i++) {
        assert(helper_func_num_calls[i] == 1);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    codset_ul->codeletset_id = codset->codeletset_id;
    assert(jbpf_codeletset_unload(codset_ul, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // We reset the helper functions. The codelet should fail to load again
    jbpf_reset_helper_functions();

    printf("Test completed successfully\n");
    return 0;
}
