// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_DEVICE_DEFS_H
#define JBPF_DEVICE_DEFS_H

#include "jbpf_common_types.h"

/**
 * @brief The maximum number of codelets that can be loaded
 * @note This is across all codeletsets and not per codeletset.
 * @ingroup core
 */
#define JBPF_MAX_LOADED_CODELETS (64)

/**
 * @brief The maximum number of codelet-sets that can be loaded
 * @ingroup core
 */
#define JBPF_MAX_LOADED_CODELETSETS (64)

/**
 * @brief The maximum number of output channels
 * @note This is a total number and not per codelet or codeletset.
 * @ingroup core
 */
#define MAX_OUTPUT_CHANNELS (JBPF_MAX_LOADED_CODELETS * 4)

/**
 * @brief The maximum number of input channels
 * @note This is a total number and not per codelet or codeletset.
 * @ingroup core
 */
#define MAX_INPUT_CHANNELS (JBPF_MAX_LOADED_CODELETS * 4)

/**
 * @brief The maximum number of the maps that can be loaded
 * @note This is a total number and not per codelet or codeletset.
 * @ingroup core
 */
#define MAX_NUM_MAPS (65535)

#ifndef JBPF_MAX_NUM_REG_THREADS
/**
 * @brief Number of supported registered threads
 * @ingroup core
 */
#define JBPF_MAX_NUM_REG_THREADS (32)
#endif

/**
 * @brief Run the maintenance thread every 1s
 * @ingroup core
 */
//
#define MAINTENANCE_MEM_CHECK_INTERVAL (10000)
#define MAINTENANCE_CHECK_INTERVAL (1000000)
#define JBPF_HEADROOM (128)

/**
 * @brief Buffer suze used for incoming jbpf requests.
 * Needs to be big, as it will store BPF programs
 * @ingroup core
 */
#define MAX_BUF_SIZE (65536)

#define JBPF_IO_ELEM_SIZE (65535 - JBPF_HEADROOM)
#define JBPF_NUM_IO_DATA_ELEM (4095U)

#define NUM_PROG_TYPES (_Prog_Type_MAX + 1)

#ifdef __cplusplus
extern thread_local jbpf_runtime_threshold_t e_runtime_threshold;
extern thread_local unsigned int seed;
#else
extern _Thread_local jbpf_runtime_threshold_t e_runtime_threshold;
extern _Thread_local unsigned int seed;
#endif

#endif // JBPF_DEVICE_DEFS