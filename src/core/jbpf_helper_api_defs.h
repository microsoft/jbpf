// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_HELPER_API_DEFS_
#define JBPF_HELPER_API_DEFS_

#include <stdint.h>

#include "jbpf_helper_api_defs_ext.h"

/**
 * @brief Success return value for map operations
 * @ingroup core
 */
#define JBPF_MAP_SUCCESS (0)

/**
 * @brief Error return value for map operations
 * @ingroup core
 */
#define JBPF_MAP_ERROR (-1)

/**
 * @brief Busy return value for map operations
 * @ingroup core
 */
#define JBPF_MAP_BUSY (-2)

/**
 * @brief Return value for map operations when map is full
 * @ingroup core
 */
#define JBPF_MAP_FULL (-4)

/**
 * @brief Maximum retry attempts for map operations
 * @ingroup core
 */
#define JBPF_MAP_RETRY_ATTEMPTS (100)

#endif /* JBPF_HELPER_API_DEFS_ */
