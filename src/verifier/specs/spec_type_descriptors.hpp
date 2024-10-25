// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include "ebpf_vm_isa.hpp"

/* struct jbpf_generic_ctx {
   uint64_t data;
   uint64_t data_end;
   uint64_t meta_data;
   uint32_t ctx_id;
   } */
/* Size of the context struct */
constexpr int jbpf_generic_regions = 3 * 8 + 4 * 1;

/* struct jbpf_stats_ctx {
   uint64_t data;
   uint64_t data_end;
   uint64_t meta_data;
   uint32_t meas_period;
   } */
/* Size of the context struct */
constexpr int jbpf_stats_regions = 3 * 8 + 4 * 1;

constexpr ebpf_context_descriptor_t jbpf_generic_descr = {jbpf_generic_regions, 0, 1 * 8, 2 * 8};
constexpr ebpf_context_descriptor_t jbpf_stats_descr = {jbpf_stats_regions, 0, 1 * 8, 2 * 8};
// If no context is given, treat it as generic
constexpr ebpf_context_descriptor_t jbpf_unspec_descr = jbpf_generic_descr;

extern const ebpf_context_descriptor_t g_jbpf_unspec_descr;
extern const ebpf_context_descriptor_t g_jbpf_generic_descr;
extern const ebpf_context_descriptor_t g_jbpf_stats_descr;
