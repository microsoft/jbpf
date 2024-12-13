// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_CONFIG_H
#define JBPF_CONFIG_H

#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "jbpf_io_defs.h"
#include "jbpf_lcm_ipc.h"
#include "jbpf_common.h"

/**
 * @brief The max length of the IPC name
 * @ingroup core
 */
#define MAX_IPC_NAME_LEN 32

/**
 * @brief Exit code for fatal errors
 * @ingroup core
 */
#define EXIT_FATAL -1

/**
 * @brief Default scheduler priority. It refers to the scheduling priority used for jbpf threads and currently is only
 * used for the maintenance thread.
 * @ingroup core
 */
#define DEFAULT_SCHED_PRIORITY 1

/**
 * @brief JBPF agent lcm IPC configuration
 * @param has_lcm_ipc_thread Whether to use LCM IPC thread
 * @param lcm_ipc_name The name of the LCM IPC socket
 * @ingroup core
 */
struct jbpf_agent_lcm_ipc_config
{
    bool has_lcm_ipc_thread;
    char lcm_ipc_name[JBPF_LCM_IPC_NAME_LEN];
};

/**
 * @brief JBPF agent thread configuration
 * @param has_affinity_agent_thread Whether to set affinity for agent thread
 * @param agent_thread_affinity_cores The bitmask of the cpuset that will be used for the agent thread
 * @param has_sched_policy_agent_thread Whether to set scheduling policy for agent thread
 * @param agent_thread_sched_policy The scheduling policy for agent thread: Set 0 for SCHED_OTHER or 1 for SCHED_FIFO
 * @param has_sched_priority_agent_thread Whether to set scheduling priority for agent thread
 * @param agent_thread_sched_priority The scheduling priority for agent thread
 * @param has_affinity_maintenance_thread Whether to set affinity for maintenance thread
 * @param maintenance_thread_affinity_cores The bitmask of the cpuset that will be used for the maintenance thread
 * @param has_sched_policy_maintenance_thread Whether to set scheduling policy for maintenance thread
 * @param maintenance_thread_sched_policy The scheduling policy for maintenance thread: Set 0 for SCHED_OTHER or 1 for
 * SCHED_FIFO
 * @param has_sched_priority_maintenance_thread Whether to set scheduling priority for maintenance thread
 * @param maintenance_thread_sched_priority The scheduling priority for maintenance thread
 * @ingroup core
 */
struct jbpf_agent_thread_config
{

    /* Configuration for affinity of jbpf agent thread */
    bool has_affinity_agent_thread;
    uint64_t agent_thread_affinity_cores;

    /* Configuration for scheduling priority of jbpf output thread */
    bool has_sched_policy_agent_thread;
    /* Set 0 for SCHED_OTHER or 1 for SCHED_FIFO */
    unsigned int agent_thread_sched_policy;
    bool has_sched_priority_agent_thread;
    unsigned int agent_thread_sched_priority;

    /* Configuration for affinity of jbpf maintenance thread */
    bool has_affinity_maintenance_thread;
    uint64_t maintenance_thread_affinity_cores;

    /* Configuration for scheduling priority of jbpf maintenance thread */
    bool has_sched_policy_maintenance_thread;
    /* Set 0 for SCHED_OTHER or 1 for SCHED_FIFO */
    unsigned int maintenance_thread_sched_policy;
    bool has_sched_priority_maintenance_thread;
    unsigned int maintenance_thread_sched_priority;
};

/**
 * @brief JBPF agent IO configuration
 * @ingroup core
 */
typedef enum jbpf_io_config_type
{
    JBPF_IO_IPC_CONFIG = 0,
    JBPF_IO_THREAD_CONFIG
} jbpf_io_config_type_t;

/**
 * @brief JBPF IO thread configuration
 * @param has_affinity_io_thread Whether to set affinity for IO thread
 * @param io_thread_affinity_cores The bitmask of the cpuset that will be used for the IO thread
 * @param has_sched_policy_io_thread Whether to set scheduling policy for IO thread
 * @param io_thread_sched_policy The scheduling policy for IO thread: Set 0 for SCHED_OTHER or 1 for SCHED_FIFO
 * @param has_sched_priority_io_thread Whether to set scheduling priority for IO thread
 * @param io_thread_sched_priority The scheduling priority for IO thread
 * @param io_mem_size The size of the memory allocated for IO thread
 * @param output_handler_ctx Context to be used by the IO thread when calling the output handler callback
 * @ingroup core
 */
struct jbpf_io_thread_config
{

    /* Configuration for affinity of jbpf IO thread */
    bool has_affinity_io_thread;
    /* Bitmask of the cpuset that will be used for the IO thread */
    uint64_t io_thread_affinity_cores;

    /* Configuration for scheduling priority of jbpf IO thread */
    bool has_sched_policy_io_thread;
    /* Set 0 for SCHED_OTHER or 1 for SCHED_FIFO */
    unsigned int io_thread_sched_policy;
    bool has_sched_priority_io_thread;
    unsigned int io_thread_sched_priority;

    size_t io_mem_size;

    /* Context to be used by the IO thread when calling the output handler callback*/
    void* output_handler_ctx;
};

/**
 * @brief JBPF IO IPC configuration
 * @note This is valid only when using jbpf in IPC mode.
 * @param ipc_mem_size The size of the memory allocated for IPC
 * @param ipc_name The name of the IPC
 * @ingroup core
 */
struct jbpf_io_ipc_config
{
    size_t ipc_mem_size;
    char ipc_name[MAX_IPC_NAME_LEN];
};

/**
 * @brief JBPF agent IO configuration
 * @param io_type The type of IO configuration
 * @param io_thread_config The IO thread configuration
 * @param io_ipc_config The IO IPC configuration
 * @ingroup core
 */
struct jbpf_agent_io_config
{

    jbpf_io_config_type_t io_type;
    union
    {
        struct jbpf_io_thread_config io_thread_config;
        struct jbpf_io_ipc_config io_ipc_config;
    };
};

/**
 * @brief JBPF agent memory configuration
 * @param mem_size The size of the memory allocated by JBPF
 * @ingroup core
 */
struct jbpf_agent_mem_config
{
    /* Memory allocated by jbpf for maps, etc.*/
    size_t mem_size;
};

/**
 * @brief JBPF agent configuration
 * @param jbpf_run_path The path to the JBPF run directory
 * @param jbpf_namespace The namespace for the JBPF agent
 * @param mem_config The memory configuration
 * @param lcm_ipc_config LCM IPC parameters
 * @param io_config The IO configuration
 * @param thread_config Thread affinity and scheduling configuration parameters
 * @ingroup core
 */
struct jbpf_config
{
    char jbpf_run_path[JBPF_RUN_PATH_LEN];

    char jbpf_namespace[JBPF_NAMESPACE_LEN];

    struct jbpf_agent_mem_config mem_config;

    /* LCM IPC parameters */
    struct jbpf_agent_lcm_ipc_config lcm_ipc_config;

    struct jbpf_agent_io_config io_config;

    /* Thread affinity and scheduling configuration parameters */
    struct jbpf_agent_thread_config thread_config;
};

/**
 * @brief Set the default configuration options
 * @param config The configuration
 * @ingroup core
 */
static inline void
jbpf_set_default_config_options(struct jbpf_config* config)
{
    strncpy(config->jbpf_run_path, JBPF_DEFAULT_RUN_PATH, JBPF_RUN_PATH_LEN - 1);
    config->jbpf_run_path[JBPF_RUN_PATH_LEN - 1] = '\0';

    strncpy(config->jbpf_namespace, JBPF_DEFAULT_NAMESPACE, JBPF_NAMESPACE_LEN - 1);
    config->jbpf_namespace[JBPF_NAMESPACE_LEN - 1] = '\0';

    config->mem_config.mem_size = JBPF_HUGEPAGE_SIZE_1GB;

    config->io_config.io_type = JBPF_IO_THREAD_CONFIG;
    config->io_config.io_thread_config.io_mem_size = JBPF_HUGEPAGE_SIZE_1GB;

    config->io_config.io_thread_config.output_handler_ctx = NULL;

    config->lcm_ipc_config.has_lcm_ipc_thread = true;
    strncpy(config->lcm_ipc_config.lcm_ipc_name, JBPF_DEFAULT_LCM_SOCKET, JBPF_LCM_IPC_NAME_LEN - 1);
    config->lcm_ipc_config.lcm_ipc_name[JBPF_LCM_IPC_NAME_LEN - 1] = '\0';

    config->thread_config.has_affinity_agent_thread = false;
    config->thread_config.has_sched_policy_agent_thread = false;
    config->thread_config.has_sched_priority_agent_thread = false;

    config->thread_config.has_affinity_maintenance_thread = false;
    config->thread_config.has_sched_policy_maintenance_thread = false;
    config->thread_config.has_sched_priority_maintenance_thread = false;
}

#endif /* JBPF_CONFIG_H */
