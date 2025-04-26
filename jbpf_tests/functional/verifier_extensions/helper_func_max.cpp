/*
 * The purpose of this test is to verify that the maximum number of helper functions can be registered.
 *
 * This test does the following:
 * 1. It defines a helper function "new_helper_implementation".
 * 2. It attempts to register this with jbpf in indexes CUSTOM_HELPER_START_ID .. MAX_HELPER_FUNC+1
 * 3. It asserts that the all of these are registered successfully except the last one.  This jbpf_tests that jbpf
 handles an attempt to register more than MAX_HELPER_FUNC helper functions.
 * 4. It registers the prototypes with the verifier for the valid indexes.
 * 5. It defines a codeletSet which uses codelet "helper_funcs/helper_funcs_max_output.o".  T When run, this codelet
 will call the helper at index given by the p->counter_a parameter.
 * 6. It asserts that the codelet passes verification.
 * 7. The hook is called 32 times.  p->counters starts with value CUSTOM_HELPER_START_ID and increments by 1 each time.
 * 8. It asserts that the helper functions were called the expected number of times, and that the data received in
 * the output channel is also correct.
 * 9. It unloads the codeletset and resets the helper functions.
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
#include "jbpf_test_lib.h"

#define NUM_ITERATIONS 5

struct packet p = {0};

int helper_func_num_calls[MAX_HELPER_FUNC] = {0};

sem_t sem;

static uint16_t processed = 0;

static jbpf_io_stream_id_t stream_id = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

#define NUM_EXPECTED_OUTPUTS (MAX_HELPER_FUNC - CUSTOM_HELPER_START_ID)

static int
new_helper_implementation(void* jbpf_ctx, int a)
{
    struct jbpf_generic_ctx* ctx = static_cast<jbpf_generic_ctx*>(jbpf_ctx);
    JBPF_UNUSED(ctx);
    assert(ctx);
    assert(a >= CUSTOM_HELPER_START_ID);
    assert(a < MAX_HELPER_FUNC);
    helper_func_num_calls[a]++;
    return 1000 + a;
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

        output = static_cast<int*>(bufs[i]);
        // printf("output=%d\n", *output);
        assert(*output >= 1000 + CUSTOM_HELPER_START_ID && *output < 1000 + MAX_HELPER_FUNC);

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

    __assert__(jbpf_init(&config) == 0);

    sem_init(&sem, 0, 0);

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Register a callback to handle the buffers sent from the codelets
    jbpf_register_io_output_cb(io_channel_check_output);

    for (uint16_t helper_id = CUSTOM_HELPER_START_ID; helper_id < MAX_HELPER_FUNC + 1; helper_id++) {
        jbpf_helper_func_def_t helper_func = {
            "new_helper_func", helper_id, (jbpf_helper_func_t)new_helper_implementation};
        helper_func.reloc_id = helper_id;
        if (helper_id < MAX_HELPER_FUNC) {
            __assert__(jbpf_register_helper_function(helper_func) == 0); // new registration
        } else {
            // check that we can't register more than MAX_HELPER_FUNC
            __assert__(jbpf_register_helper_function(helper_func) < 0); // failure
        }
    }

    for (uint16_t helper_id = CUSTOM_HELPER_START_ID; helper_id < MAX_HELPER_FUNC; helper_id++) {
        // We first need to define a prototype for the function
        static const struct EbpfHelperPrototype jbpf_new_helper_proto = {
            .name = "new_helper_implementation_proto",
            .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer
            .argument_type =
                {
                    EBPF_ARGUMENT_TYPE_PTR_TO_CTX, // The first argument is a map pointer
                    EBPF_ARGUMENT_TYPE_ANYTHING,   // The next three arguments are generic values
                    EBPF_ARGUMENT_TYPE_ANYTHING,
                    EBPF_ARGUMENT_TYPE_ANYTHING,
                },
        };

        // Next, we register the prototype and link it to the map type
        jbpf_verifier_register_helper(helper_id, jbpf_new_helper_proto);
    }

    // The name of the codeletset
    strcpy(codset->codeletset_id.name, "helper_funcs_max_output_codeletset");

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
    __assert__(jbpf_path != NULL);
    snprintf(
        cod->codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/helper_funcs/helper_funcs_max_output.o",
        jbpf_path);
    strcpy(cod->codelet_name, "helper_funcs_max_output");
    strcpy(cod->hook_name, "test1");

    // The test should still fail
    auto result = jbpf_verify(cod->codelet_path, nullptr, nullptr);
    printf("result.verification_pass: %d\n", result.verification_pass);
    assert(result.verification_pass);

    // Load the codeletset. Loading should fail
    __assert__(jbpf_codeletset_load(codset, NULL) == JBPF_CODELET_LOAD_SUCCESS);

    // Call hook
    for (int i = CUSTOM_HELPER_START_ID; i < MAX_HELPER_FUNC; i++) {
        p.counter_a = i;
        hook_test1(&p, 1);
    }
    // Make sure that the helper was called expected number of times
    for (int i = CUSTOM_HELPER_START_ID; i < MAX_HELPER_FUNC; i++) {
        assert(helper_func_num_calls[i] == 1);
    }

    // Wait for all the output checks to finish
    sem_wait(&sem);

    codset_ul->codeletset_id = codset->codeletset_id;
    __assert__(jbpf_codeletset_unload(codset_ul, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // We reset the helper functions. The codelet should fail to load again
    jbpf_reset_helper_functions();

    printf("Test completed successfully\n");
    return 0;
}
