// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_COMMON_TYPES_H
#define JBPF_COMMON_TYPES_H

/**
 * @brief Length of the jbpf hook name.
 * @details This is the maximum length of the hook name.
 * @ingroup core
 */
#define JBPF_HOOK_NAME_LEN (256U)

/**
 * @brief Type representing the name of a jbpf hook.
 * @ingroup core
 */
typedef char jbpf_hook_name_t[JBPF_HOOK_NAME_LEN];

/**
 * @brief Default priority for a jbpf codelet.
 * @details This is the default priority for a jbpf codelet.
 * @ingroup core
 */
#define JBPF_CODELET_PRIORITY_DEFAULT (1U)

/**
 * @brief Priority of the codelet.
 * @details The priority is used to determine the order in which the codelets are executed.
 * @note The lowest priority is 0, the highest priority is UINT_MAX.
 * The default priority is defined by `JBPF_CODELET_PRIORITY_DEFAULT`.
 * @ingroup core
 */
typedef uint32_t jbpf_codelet_priority_t;

/**
 * @brief Runtime threshold type for jbpf.
 * @details This represents the time threshold in microseconds.
 * @note Value of 0 means no threshold checking is performed.
 * @ingroup core
 */
typedef uint64_t jbpf_runtime_threshold_t;

/**
 * @brief Length of the jbpf IO channel name.
 * @details This is the maximum length of the IO channel name.
 * @ingroup core
 */
#define JBPF_IO_CHANNEL_NAME_LEN (256U)

/**
 * @brief Type representing the name of a jbpf IO channel.
 * @ingroup core
 */
typedef char jbpf_io_channel_name_t[JBPF_IO_CHANNEL_NAME_LEN];

#endif /* JBPF_COMMON_TYPES_H */
