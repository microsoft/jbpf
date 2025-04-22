/*
 * This test does the following:
 * 1. It tries to load a codelet that uses an unregistered external function and the load fails.
 * 2. It registers the external helper function. The loading of the codelet now succeeds.
 * 3. It checks that the external helper function is called correctly.
 * 4. It overrides an existing helper function to change its implementation.
 * 5. It resets the jbpf helper functions to default. The codelet loading should fail again.
 */
#include <assert.h>
#include <semaphore.h>

#include "jbpf.h"
#include "jbpf_agent_common.h"
#include "jbpf_verifier_extension_defs.h"
#include "jbpf_utils.h"
#include "jbpf_helper_api_defs_ext.h"

// Contains the struct and hook definitions
#include "jbpf_test_def.h"

static bool helper_function_called = false;
static bool get_time_ns_replaced = false;

static int
new_helper_implementation(void* jbpf_ctx, int a, int b, int c)
{
    struct jbpf_generic_ctx* ctx = jbpf_ctx;

    JBPF_UNUSED(ctx);

    assert(ctx);
    assert(ctx->ctx_id == 999);

    assert(a == 100 && b == 200 && c == 300);
    helper_function_called = true;

    return a + b + c;
}

static uint64_t
jbpf_time_get_ns_replacement(void)
{
    get_time_ns_replaced = true;
    return 0;
}

jbpf_io_stream_id_t stream_id_c1 = {
    .id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

int
main(int argc, char** argv)
{
    struct jbpf_codeletset_load_req codeletset_req_c1 = {0};
    struct jbpf_codeletset_unload_req codeletset_unload_req_c1 = {0};
    const char* jbpf_path = getenv("JBPF_PATH");
    struct jbpf_config config = {0};

    jbpf_set_default_config_options(&config);

    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    int res = jbpf_init(&config);
    assert(res == 0);
#ifdef NDEBUG
    (void)res; // suppress unused-variable warning in release mode
#endif

    // The thread will be calling hooks, so we need to register it
    jbpf_register_thread();

    // Load the codeletset with codelet C1 in hook "test1"

    // The name of the codeletset
    strcpy(codeletset_req_c1.codeletset_id.name, "new_helper_codeletset");

    // We have only one codelet in this codeletset
    codeletset_req_c1.num_codelet_descriptors = 1;

    // The codelet has just one output channel and no shared maps
    codeletset_req_c1.codelet_descriptor[0].num_in_io_channel = 0;
    codeletset_req_c1.codelet_descriptor[0].num_out_io_channel = 0;

    // The path of the codelet
    assert(jbpf_path != NULL);
    snprintf(
        codeletset_req_c1.codelet_descriptor[0].codelet_path,
        JBPF_PATH_LEN,
        "%s/jbpf_tests/test_files/codelets/new_helper/new_helper.o",
        jbpf_path);
    strcpy(codeletset_req_c1.codelet_descriptor[0].codelet_name, "new_helper");
    strcpy(codeletset_req_c1.codelet_descriptor[0].hook_name, "test1");

    // Load the codeletset. Loading should fail
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_CREATION_FAIL);

    jbpf_helper_func_def_t helper_func = {
        "new_helper_func", CUSTOM_HELPER_START_ID, (jbpf_helper_func_t)new_helper_implementation};

    JBPF_UNUSED(helper_func);

    // Try to register a function with relocation id that is out of range
    helper_func.reloc_id = 0;
    assert(jbpf_register_helper_function(helper_func) == -1);
    helper_func.reloc_id = 1000;
    assert(jbpf_register_helper_function(helper_func) == -1);
    // Now register the proper one
    helper_func.reloc_id = CUSTOM_HELPER_START_ID;
    assert(jbpf_register_helper_function(helper_func) == 0);

    // Codelet loading should now succeed
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_LOAD_SUCCESS);

    // Call hook and make sure that the helper was called
    struct packet p = {0};
    hook_test1(&p, 999);
    assert(helper_function_called);
    helper_function_called = false;

    // Unload the codeletset
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "new_helper_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // Now we will replace the helper function get_time_ns with a custom one
    jbpf_helper_func_def_t helper_func_replacement = {
        "helper_func_replacement", JBPF_TIME_GET_NS, (jbpf_helper_func_t)jbpf_time_get_ns_replacement};

    JBPF_UNUSED(helper_func_replacement);
    assert(jbpf_register_helper_function(helper_func_replacement) == 1);

    // We load the codelet again and see that it loads and the function has been replaced
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_LOAD_SUCCESS);

    // Call hook and make sure that the helper was called
    hook_test1(&p, 999);
    assert(helper_function_called);
    assert(get_time_ns_replaced);
    strcpy(codeletset_unload_req_c1.codeletset_id.name, "new_helper_codeletset");
    assert(jbpf_codeletset_unload(&codeletset_unload_req_c1, NULL) == JBPF_CODELET_UNLOAD_SUCCESS);

    // We reset the helper functions. The codelet should fail to load again
    jbpf_reset_helper_functions();
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_CREATION_FAIL);

    // Deregister an invalid helper function
    assert(jbpf_deregister_helper_function(1000) == -2);

    // Deregister the registered helper function
    assert(jbpf_deregister_helper_function(JBPF_TIME_GET_NS) == 0);

    // Since the helper function is no longer available, the test should fail
    res = jbpf_codeletset_load(&codeletset_req_c1, NULL);
    assert(res == JBPF_CODELET_CREATION_FAIL);

    // Function is already deregistered
    assert(jbpf_deregister_helper_function(JBPF_TIME_GET_NS) == -1);

    printf("Test completed successfully\n");
    return 0;
}
