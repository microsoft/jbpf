// Copyright (c) Microsoft Corporation. All rights reserved.
#include "platform.hpp"
#include <bits/stdint-uintn.h>
#include "spec_type_descriptors.hpp"

using namespace prevail;

const ebpf_context_descriptor_t g_jbpf_unspec_descr = jbpf_unspec_descr;
const ebpf_context_descriptor_t g_jbpf_generic_descr = jbpf_generic_descr;
const ebpf_context_descriptor_t g_jbpf_stats_descr = jbpf_stats_descr;

/* Here we define all the prototypes for the helper functions used by jbpf */

#define EBPF_RETURN_TYPE_PTR_TO_ALLOC_MEM_OR_NULL EBPF_RETURN_TYPE_UNSUPPORTED
#define EBPF_ARGUMENT_TYPE_CONST_ALLOC_SIZE_OR_ZERO EBPF_ARGUMENT_TYPE_UNSUPPORTED
#define EBPF_ARGUMENT_TYPE_PTR_TO_ALLOC_MEM EBPF_ARGUMENT_TYPE_UNSUPPORTED

static const struct EbpfHelperPrototype jbpf_unspec_proto = {
    .name = "unspec",
};

/* Always built-in helper functions. */

/*
 * void *jbpf_map_lookup_elem(&map, &key)
 *     Return: Map value or NULL
 */
static const struct EbpfHelperPrototype jbpf_map_lookup_elem_proto = {
    .name = "map_lookup_elem",
    .return_type = EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

/*
 * void *jbpf_map_lookup_reset_elem(&map, &key)
 *     Return: Map value or NULL
 */
static const struct EbpfHelperPrototype jbpf_map_lookup_reset_elem_proto = {
    .name = "map_lookup_reset_elem",
    .return_type = EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

/*
 * int jbpf_map_update_elem(&map, &key, &value, flags)
 *     Return: 0 on success or negative error
 */
static const struct EbpfHelperPrototype jbpf_map_update_elem_proto = {
    .name = "map_update_elem",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY,
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP_VALUE,
            EBPF_ARGUMENT_TYPE_ANYTHING,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

/*
 * int jbpf_map_delete_elem(&map, &key)
 *     Return: 0 on success or negative error
 */
static const struct EbpfHelperPrototype jbpf_map_delete_elem_proto = {
    .name = "map_delete_elem",
    //.gpl_only	= false,
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP_KEY,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

/*
 * int jbpf_map_dump(&map, &key)
 *     Return: number of elements dumped on success or 0
 */
static const struct EbpfHelperPrototype jbpf_map_dump_proto = {
    .name = "map_dump",
    //.gpl_only	= false,
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_WRITABLE_MEM,
            EBPF_ARGUMENT_TYPE_CONST_SIZE,
            EBPF_ARGUMENT_TYPE_ANYTHING,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

/*
 * int jbpf_map_clear(&map)
 *     Return: 0 on success or 1 if map could not be cleared
 */
static const struct EbpfHelperPrototype jbpf_map_clear_proto = {
    .name = "map_clear",
    //.gpl_only	= false,
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
        },
};

/*
 * u64 jbpf_time_get_ns()
 *     Return: current time
 */
static const struct EbpfHelperPrototype jbpf_time_get_ns_proto = {
    .name = "time_get_ns",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
};

/*
 * u64 jbpf_get_sys_time()
 *     Return: current system time (internal representation)
 */
static const struct EbpfHelperPrototype jbpf_get_sys_time_proto = {
    .name = "time_get_sys_time",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
};

/*
 * u64 jbpf_get_sys_time_diff_ns()
 *     Return: rent system time (internal representation)
 */
static const struct EbpfHelperPrototype jbpf_get_sys_time_diff_ns_proto = {
    .name = "time_get_sys_time_diff_ns",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_ANYTHING,
            EBPF_ARGUMENT_TYPE_ANYTHING,
        },
};

static const struct EbpfHelperPrototype jbpf_hash_proto = {
    .name = "hash",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM,
            EBPF_ARGUMENT_TYPE_CONST_SIZE,
        },
};

static const struct EbpfHelperPrototype jbpf_printf_proto = {
    .name = "printf",
    .return_type = EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM,
            EBPF_ARGUMENT_TYPE_CONST_SIZE,
        },
};

static const struct EbpfHelperPrototype jbpf_ringbuf_output_proto = {
    .name = "ringbuf_output",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_READABLE_MEM,
            EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
            EBPF_ARGUMENT_TYPE_DONTCARE,
            EBPF_ARGUMENT_TYPE_DONTCARE,
        },
};

static const struct EbpfHelperPrototype jbpf_mark_runtime_init_proto = {
    .name = "mark_runtime_init",
    .return_type = EBPF_RETURN_TYPE_INTEGER_OR_NO_RETURN_IF_SUCCEED,
    .argument_type =
        {

        },
};

static const struct EbpfHelperPrototype jbpf_runtime_limit_exceeded_proto = {
    .name = "runtime_limit_exceeded",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {

        },
};

static const struct EbpfHelperPrototype jbpf_rand_proto = {
    .name = "rand",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {

        },
};

static const struct EbpfHelperPrototype jbpf_control_input_receive_proto = {
    .name = "control_input_receive",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
            EBPF_ARGUMENT_TYPE_PTR_TO_WRITABLE_MEM,
            EBPF_ARGUMENT_TYPE_CONST_SIZE_OR_ZERO,
        },
};

static const struct EbpfHelperPrototype jbpf_get_output_buf_proto = {
    .name = "get_output_buf",
    .return_type = EBPF_RETURN_TYPE_PTR_TO_MAP_VALUE_OR_NULL,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
        },
};

static const struct EbpfHelperPrototype jbpf_send_output_proto = {
    .name = "send_output",
    .return_type = EBPF_RETURN_TYPE_INTEGER,
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP,
        },
};

#define FN(x) jbpf_##x##_proto
// keep this on a round line
std::vector<struct EbpfHelperPrototype> prototypes = {
    FN(unspec),
    FN(map_lookup_elem),
    FN(map_lookup_reset_elem),
    FN(map_update_elem),
    FN(map_delete_elem),
    FN(map_clear),
    FN(map_dump),
    FN(time_get_ns),
    FN(get_sys_time),
    FN(get_sys_time_diff_ns),
    FN(hash),
    FN(printf),
    FN(ringbuf_output),
    FN(mark_runtime_init),
    FN(runtime_limit_exceeded),
    FN(rand),
    FN(control_input_receive),
    FN(get_output_buf),
    FN(send_output),
    /* EXTEND WITH THE NEW PROTOTYPES HERE */
};

void
jbpf_verifier_register_helper(int helper_id, struct EbpfHelperPrototype prototype)
{
    if (helper_id >= prototypes.size()) {
        prototypes.resize(helper_id + 1);
    }
    prototypes[helper_id] = prototype;
}

bool
jbpf_verifier_is_helper_usable(int32_t n)
{
    if (n >= prototypes.size() || n < 0)
        return false;

    // If the helper has a context_descriptor, it must match the hook's context_descriptor.
    if ((prototypes[n].context_descriptor != nullptr) &&
        (prototypes[n].context_descriptor != thread_local_program_info->type.context_descriptor))
        return false;

    return true;
}

EbpfHelperPrototype
jbpf_verifier_get_helper_prototype(int32_t n)
{
    if (!jbpf_verifier_is_helper_usable(n))
        throw std::exception();
    return prototypes[n];
}
