// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef JBPF_HELPER_H
#define JBPF_HELPER_H

#include <stdint.h>
#include <stdbool.h>

#include "jbpf_defs.h"
#include "jbpf_helper_api_defs.h"
#include "jbpf_helper_utils.h"

/**
 * @brief Perform a lookup in map for a value associated kith key
 * @param map The map to perform the lookup in
 * @param key The key to lookup
 * @return ored value for the given key if it exists in the map or NULL otherwise
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static void* (*jbpf_map_lookup_elem)(const void*, const void*) = (void* (*)(const void*, const void*))JBPF_MAP_LOOKUP;

/**
 * @brief Perform a lookup in map for a value associated kith key and reset the value
 * @param map The map to perform the lookup in
 * @param key The key to lookup
 * @return ored value for the given key if it exists in the map or NULL otherwise
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static void* (*jbpf_map_lookup_reset_elem)(const void*, const void*) = (void* (*)(const void*, const void*))
    JBPF_MAP_LOOKUP_RESET;

/**
 * @brief Adds or updates the value associated with key in map with the value item. The flags argument is currently
 * ignored.
 * @param map The map to add or update the value in.
 * @param key The key to add or update.
 * @param value The value to add or update.
 * @param flags Flags (currently unused, set to 0).
 * @return 0 if the value was added or updated successfully or a negative value otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_map_update_elem)(void*, const void*, void*, uint64_t) = (int (*)(void*, const void*, void*, uint64_t))
    JBPF_MAP_UPDATE;

/**
 * @brief Deletes the value associated with key in map.
 * @param map The map to delete the value from.
 * @param key The key to delete.
 * @return 0 if the value was deleted successfully or a negative value otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_map_delete_elem)(void*, const void*) = (int (*)(void*, const void*))JBPF_MAP_DELETE;

/**
 * @brief Clears all value in the map.
 * @param map The map to clear.
 * @return 0 if the map was cleared successfully or a negative value otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_map_clear)(const void*) = (int (*)(const void*))JBPF_MAP_CLEAR;

/**
 * @brief Dumps the contents of the map into the data buffer.
 * @param map The map to dump.
 * @param data The buffer to dump the data into.
 * @param max_size The maximum size of the buffer.
 * @param flags Flags (currently unused, set to 0).
 * @return 0 if the map was dumped successfully or a negative value otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_map_dump)(void*, void*, uint32_t, uint64_t) = (int (*)(void*, void*, uint32_t, uint64_t))
    JBPF_MAP_DUMP;

/**
 * @brief Returns the time in nanoseconds. Uses clock_gettime(CLOCK_REALTIME) internally.
 * @return The current time in nanoseconds.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static uint64_t (*jbpf_time_get_ns)(void) = (uint64_t(*)(void))JBPF_TIME_GET_NS;

/**
 * @brief Returns the time in nanoseconds. Uses clock_gettime(CLOCK_MONOTONIC_RAW) internally.
 * @param is_start when set to true, returns the jbpf_measure_start_time otherwise returns the jbpf_measure_stop_time.
 * @return The time in nanoseconds.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static uint64_t (*jbpf_get_sys_time)(bool) = (uint64_t(*)(bool))JBPF_GET_SYS_TIME;

/**
 * @brief Returns the time difference between two times in nanoseconds.
 * @param start_time The start time.
 * @param end_time The end time.
 * @return The time difference in nanoseconds.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static uint64_t (*jbpf_get_sys_time_diff_ns)(uint64_t, uint64_t) = (uint64_t(*)(uint64_t, uint64_t))
    JBPF_GET_SYS_TIME_DIFF_NS;

/**
 * @brief Creates a hash for an arbitrary item of a given size.
 * @param item The item to hash.
 * @param len The length of the item.
 * @return A hash value for item.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static uint32_t (*jbpf_hash)(void*, uint64_t) = (uint32_t(*)(void*, uint64_t))JBPF_HASH;

/**
 * @brief Prints a formatted string fmt of size len (Currently not supported)
 * Takes up to 3 arguments.
 * This function can add significant overhead and therefore should not be used in programs that are expected to be
 * loaded in the fast path. Should be mainly used for debugging purposes.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static void (*jbpf_printf)(const void*, uint32_t, ...) = (void (*)(const void*, uint32_t, ...))JBPF_PRINTF;

/**
 * @brief Outputs data of size from a map of type JBPF_MAP_TYPE_RINGBUF through the output thread of JBPF. It is up to
 * the user to ensure that the data is valid for the output schema defined for the map. If the data is invalid,
 * JBPF will output garbage that will not be decodable. This function performs two operations; it reserves size memory
 * in the ringbuffer and then does a memcpy() of the data to the reserved memory. If a program requires minimum overhead
 * with zero-copy for outputs, it should use jbpf_ringbuf_reserve and jbpf_ringbuf_submit instead.
 * @return 0 if the data is placed in the ringbuf succesfully or -1 otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_ringbuf_output)(const void*, void*, uint64_t) = (int (*)(const void*, void*, uint64_t))
    JBPF_RINGBUF_OUTPUT;

/**
 * @brief Receives control input data and copies them in some memory region.
 * @return 0 if control input was received successfully and non-zero otherwise.
 * @incgroup jbpf_agent
 * @incgroup helper_function
 */
static int (*jbpf_control_input_receive)(void*, void*, uint32_t) = (int (*)(void*, void*, uint32_t))
    JBPF_CONTROL_INPUT_RECEIVE;

/**
 * @brief Obtains a memory chunk from an output map, which can be used to send data out.
 * This call will keep returning the same memory chunk if called multiple times in a row, unless jbpf_send_output()
 * is called, to push the memory to the output.
 * @return Pointer to the allocated memory if request was successful or NULL if there is no memory available.
 * @incgroup jbpf_agent
 * @incgroup helper_function
 */
static void* (*jbpf_get_output_buf)(void*) = (void* (*)(void*))JBPF_GET_OUTPUT_BUF;

/**
 * @brief Pushes an allocated memory chunk to the designated map to be sent as output.
 * The function jbpf_get_output_buf() must be called first, to get a pointer to some allocated memory region.
 * This call does not require the pointer to the allocated memory to be passed as an argument.
 * @return 0 if the output was sent successfully and a negative number if the request failed (e.g., no memory was
 * previously allocated).
 * @incgroup jbpf_agent
 * @incgroup helper_function
 */
static int (*jbpf_send_output)(void*) = (int (*)(void*))JBPF_SEND_OUTPUT;

/**
 * @brief Adds a checkpoint for measuring elapsed runtime.
 * This is a stateful call and is intended to be used along with jbpf_check_runtime_limit to check if a codelet has
 * exceeded some runtime threshold.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static void (*jbpf_mark_runtime_init)(void) = (void (*)(void))JBPF_MARK_RUNTIME_INIT;

/**
 * @brief Checks whether the running codelet has exceeded some runtime threshold, set during the loading phase.
 * This must be used in conjunction to jbpf_mark_runtime_init
 * @return 0 if the runtime threshold has not been exceeded, and a non-zero value otherwise.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_check_runtime_limit)(void) = (int (*)(void))JBPF_RUNTIME_LIMIT_EXCEEDED;

/**
 * @brief Returns a random integer.
 * @return A random int.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
static int (*jbpf_rand)(void) = (int (*)(void))JBPF_RAND;

/**
 * Macros to repeat jbpf_map_update_elem several times, if the map is busy.
 * param map The map to perform the operation on.
 * param key The key to perform the operation on.
 * param value The value to perform the operation on.
 * param flags currently unused (set to 0).
 * return 0 if the operation was successful or a negative value otherwise.
 */
#define jbpf_map_try_update_elem(map, key, value, flags)                                  \
    ({                                                                                    \
        int result;                                                                       \
        for (int attempt = 0; attempt < JBPF_MAP_RETRY_ATTEMPTS; attempt++) {             \
            if ((result = jbpf_map_update_elem(map, key, value, flags)) != JBPF_MAP_BUSY) \
                break;                                                                    \
        }                                                                                 \
        result;                                                                           \
    })

/**
 * Macros to repeat jbpf_map_try_delete_elem several times, if the map is busy.
 * param map The map to perform the operation on.
 * param key The key to perform the operation on.
 * return 0 if the operation was successful or a negative value otherwise.
 */
#define jbpf_map_try_delete_elem(map, key)                                    \
    ({                                                                        \
        int result;                                                           \
        for (int attempt = 0; attempt < JBPF_MAP_RETRY_ATTEMPTS; attempt++) { \
            if ((result = jbpf_map_delete_elem(map, key)) != JBPF_MAP_BUSY)   \
                break;                                                        \
        }                                                                     \
        result;                                                               \
    })

/**
 * Macros to repeat jbpf_map_clear several times, if the map is busy.
 * if the runtime threshold is exceeded, while re-trying.
 * param map The map to perform the operation on.
 * return 0 if the operation was successful or a negative value otherwise.
 */
#define jbpf_map_try_clear(map)                                               \
    ({                                                                        \
        int result;                                                           \
        for (int attempt = 0; attempt < JBPF_MAP_RETRY_ATTEMPTS; attempt++) { \
            if ((result = jbpf_map_clear(map)) != JBPF_MAP_BUSY)              \
                break;                                                        \
        }                                                                     \
        result;                                                               \
    })

/**
 * brief Macros to repeat jbpf_map_dump several times, if the map is busy.
 * if the runtime threshold is exceeded, while re-trying.
 * param map The map to perform the operation on.
 * param data The data to perform the operation on.
 * param max_size The maximum size to perform the operation on.
 * param flags Flags (currently unused, set to 0).
 * return 0 if the operation was successful or a negative value otherwise.
 */
#define jbpf_map_try_dump(map, data, max_size, flags)                                  \
    ({                                                                                 \
        int result;                                                                    \
        for (int attempt = 0; attempt < JBPF_MAP_RETRY_ATTEMPTS; attempt++) {          \
            if ((result = jbpf_map_dump(map, data, max_size, flags)) != JBPF_MAP_BUSY) \
                break;                                                                 \
        }                                                                              \
        result;                                                                        \
    })

#ifdef JBPF_DEBUG_ENABLED

/**
 * @brief Prints a formatted string.
 * @note This supports up to 3 arguments for the printed string.
 * This function can add significant overhead and therefore should not be used in programs that are expected to be
 * loaded in the fast path. Should be mainly used for debugging purposes.
 * @ingroup jbpf_agent
 * @ingroup helper_function
 */
#define jbpf_printf_debug(fmt, ...)                           \
    ({                                                        \
        char ____fmt[] = fmt;                                 \
        jbpf_printf(____fmt, sizeof(____fmt), ##__VA_ARGS__); \
    })

#else

#define jbpf_printf_debug(fmt, ...) ((void)0)

#endif

#endif
