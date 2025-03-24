// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_H_
#define JBPF_H_

#include <stddef.h>
#include <stdint.h>

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "jbpf_config.h"
#include "jbpf_lcm_api.h"
#include "jbpf_io_channel.h"

#include "ck_epoch.h"

    typedef void (*jbpf_io_output_handler_cb)(jbpf_io_stream_id_t* stream_id, void** bufs, int nbuf, void* ctx);

#ifdef __cplusplus
    extern thread_local int __thread_id;
    extern thread_local ck_epoch_record_t* e_record;
#else
extern _Thread_local int __thread_id;
extern _Thread_local ck_epoch_record_t* e_record;
#endif

#define MAX_HELPER_FUN_NAME_LEN 64

    typedef uint64_t (*jbpf_helper_func_t)(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4);

    /**
     * @brief Structure to define a helper function
     * @ingroup helper_function
     * @ingroup core
     * @param name The name of the function
     * @param reloc_id The relocation id of the function
     * @param function_cb The function callback
     * @ingroup helper_function
     */
    typedef struct jbpf_helper_func_def
    {
        char name[MAX_HELPER_FUN_NAME_LEN];
        int reloc_id;
        jbpf_helper_func_t function_cb;
    } jbpf_helper_func_def_t;

    /**
     * @brief Initializes jbpf using default config parameters.
     *
     * This function should be used to initialize jbpf with the default configuration parameters.
     * The default parameters set the IP and port of the output interface (for collection of data
     * from ringbuf outputs) to localhost and 20788 respectively. It also sets the listening port
     * of the agent to 30590. This function should be called only once.
     *
     * @return -1 if initialization failed, -2 if there was a problem in the initialization of output
     * channels and -3 if there was an error in the initialization of of the agent network interfaces.
     * @ingroup jbpf_agent
     */
    int
    jbpf_init_default(void);

    /**
     * @brief Initializes jbpf using custom config.
     *
     * This function should be used to initialize jbpf with the custom configuration parameters.
     *
     * @param config Pointer to a jbpf_config struct containing the configuration parameters to be set.
     * @return -1 if initialization failed, -2 if there was a problem in the initialization of output
     * channels and -3 if there was an error in the initialization of of the agent network interfaces.
     * This function should be called only once.
     * @ingroup jbpf_agent
     */
    int
    jbpf_init(struct jbpf_config* config);

    /**
     * @brief Initializes threads that will be used to call jbpf hooks.
     *
     * This function should be used to initialize any thread that will be used to call a jbpf hook.
     *
     * @ingroup jbpf_agent
     */
    void
    jbpf_register_thread(void);

    /**
     * @brief Perform jbpf maintenance tasks
     *
     * This function should be called periodically from each thread that is calling jbpf hooks.
     *
     * @ingroup jbpf_agent
     */
    void
    jbpf_maintenance(void);

    /**
     * @brief Initializes threads that will be used to call jbpf hooks.
     *
     * This function should be used to cleanup any thread that was used to call a jbpf hook.
     *
     * @ingroup jbpf_agent
     */
    void
    jbpf_cleanup_thread(void);

    /**
     * @brief Stop the jbpf agent for a graceful exit.
     *
     * This function releases the resources allocated to the jbpf agent.
     * It should be called when exiting and after calling one of the jbpf_init() or jbpf_init_default()
     * @ingroup jbpf_agent
     */
    void
    jbpf_stop(void);

    /**
     * @brief Register a callback function for handling output messages in IO standalone mode
     */
    void jbpf_register_io_output_cb(jbpf_io_output_handler_cb);

    /**
     * @brief Registers a helper function to jbpf
     *
     * @param func The descriptor of the function to be registered
     * @return int 0 if the function was registered successfully, 1
     * if the function replaced an existing one and -1, if the relocation
     * id was 0 or our of range
     * @ingroup helper_function
     * @ingroup core
     */
    int
    jbpf_register_helper_function(jbpf_helper_func_def_t func);

    /**
     * @brief Removes a helper function from the list of the available helper functions.
     * Once removed, the helper function will not be used during the creation of new VMs
     * @param reloc_id The relocation id of the function to be removed.
     * @return int 0, if the function was removed successfully, -1, if no function was registered
     * to the given relocation id and -2, if the relocation id was out of range.
     * @ingroup helper_function
     * @ingroup core
     */
    int
    jbpf_deregister_helper_function(int reloc_id);

    /**
     * @brief Resets the set of helper functions to the default one that comes built-in with jbpf
     *
     */
    void
    jbpf_reset_helper_functions(void);

    /**
     * @brief Loads a codeletset
     * @param load_req The request containing all the details about the codeletset to be loaded.
     * @param err A structure to store a return message if loading fails.
     * @return int 0 if load was successful or a negative value otherwise.
     * @ingroup core
     * @ingroup lcm
     */
    int
    jbpf_codeletset_load(struct jbpf_codeletset_load_req* load_req, jbpf_codeletset_load_error_s* err);

    /**
     * @brief Unloads a codeletset
     * @param unload_req The request containing all the details about the codeletset to be unloaded.
     * @param err  A structure to store a return message if unloading fails
     * @return int 0 if load was successful or a negative value otherwise.
     * @ingroup core
     * @ingroup lcm
     */
    int
    jbpf_codeletset_unload(struct jbpf_codeletset_unload_req* unload_req, jbpf_codeletset_load_error_s* err);

    /**
     * @brief get the io context
     * @return struct jbpf_io_ctx* The io context
     * @ingroup io
     */
    struct jbpf_io_ctx*
    jbpf_get_io_ctx(void);

    /**
     * @brief Sends an input message to a jbpf agent
     * @param stream_id The stream id of the message
     * @param data The data to be sent
     * @param size The size of the data
     * @return int 0 if the message was sent successfully, -1 otherwise
     * @ingroup io
     * @ingroup jbpf_agent
     * @ingroup core
     */
    int
    jbpf_send_input_msg(jbpf_io_stream_id_t* stream_id, void* data, size_t size);

    /**
     * @defgroup jbpf_agent   jbpf Agent API
     * API to interact with a jbpf agent
     *
     * @defgroup core   JBPF Core API
     * API to interact with the JBPF core
     *
     * @defgroup io   JBPF IO Library
     * JBPF IO Library
     *
     * @defgroup lcm   JBPF Load Control Module API
     * JBPF Load Control Module API
     *
     * @defgroup logger   JBPF Logging
     * JBPF Logging
     *
     * @defgroup mem_mgmt   JBPF Memory Management
     * JBPF Memory Management
     *
     * @defgroup verifier   JBPF Verifier
     * JBPF Verifier
     *
     * @defgroup helper_function   JBPF Helper Functions
     * JBPF Helper Functions
     *
     * @defgroup experimental   JBPF Experimental Features
     * JBPF Experimental Features
     *
     * @defgroup build_params   JBPF Build Parameters
     * JBPF Build Parameters
     *
     * @defgroup hooks   JBPF Hooks
     * JBPF Hooks
     */

    __attribute__((always_inline)) static int inline get_jbpf_hook_thread_id(void) { return __thread_id; }

#ifdef __cplusplus
}
#endif

#endif
