// Copyright (c) Microsoft Corporation. All rights reserved.
#include <stdexcept>

#define PTYPE(name, descr, native_type, prefixes) \
    {                                             \
        name, descr, native_type, prefixes        \
    }
#define PTYPE_PRIVILEGED(name, descr, native_type, prefixes) \
    {                                                        \
        name, descr, native_type, prefixes, true             \
    }

#include "crab_verifier.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "jbpf_platform.hpp"
#include "specs/spec_type_descriptors.hpp"
#include "jbpf_defs.h"

#include <asm_files.hpp>

static int
create_map_jbpf(
    uint32_t map_type, uint32_t key_size, uint32_t value_size, uint32_t max_entries, ebpf_verifier_options_t options);

// Allow for comma as a separator between multiple prefixes, to make
// the preprocessor treat a prefix list as one macro argument.
#define COMMA ,

const EbpfProgramType jbpf_unspec_program_type = PTYPE("unspec", &g_jbpf_unspec_descr, JBPF_PROG_TYPE_UNSPEC, {});

std::vector<EbpfProgramType> jbpf_program_types = {
    jbpf_unspec_program_type,
    PTYPE("jbpf_generic", &g_jbpf_generic_descr, JBPF_PROG_TYPE_GENERIC, {"jbpf_generic"}),
    PTYPE("jbpf_stats", &g_jbpf_stats_descr, JBPF_PROG_TYPE_STATS, {"jbpf_stats"}),
};

void
jbpf_verifier_register_program_type(int prog_type_id, EbpfProgramType program_type)
{
    if (prog_type_id >= jbpf_program_types.size()) {
        jbpf_program_types.resize(prog_type_id + 1);
    }
    jbpf_program_types[prog_type_id] = program_type;
}

static EbpfProgramType
jbpf_verifier_get_program_type(const std::string& section, const std::string& path)
{
    EbpfProgramType type{};

    for (const EbpfProgramType& t : jbpf_program_types) {
        for (const std::string& prefix : t.section_prefixes) {
            if (section.find(prefix) == 0)
                return t;
        }
    }

    return jbpf_unspec_program_type;
}

#ifdef __linux__
#define JBPF_MAP_TYPE(x) JBPF_MAP_TYPE_##x, #x
#else
#define JBPF_MAP_TYPE(x) 0, #x
#endif

std::vector<EbpfMapType> jbpf_map_types = {
    {JBPF_MAP_TYPE(UNSPEC)},
    {JBPF_MAP_TYPE(ARRAY), true}, // True means that key is integer in range [0, max_entries-1]
    {JBPF_MAP_TYPE(HASHMAP)},
    {JBPF_MAP_TYPE(RINGBUF)},
    {JBPF_MAP_TYPE(CONTROL_INPUT)},
    {JBPF_MAP_TYPE(PER_THREAD_ARRAY), true},
    {JBPF_MAP_TYPE(PER_THREAD_HASHMAP)},
    {JBPF_MAP_TYPE(OUTPUT)},
};

int
jbpf_verifier_register_map_type(int map_id, EbpfMapType map_type)
{
    // EbpfMapValueType::PROGRAM is not currently supported
    if (map_type.value_type == EbpfMapValueType::PROGRAM) {
        return -1;
    }

    if (map_id >= jbpf_map_types.size()) {
        jbpf_map_types.resize(map_id + 1);
    }
    jbpf_map_types[map_id] = map_type;
    return 0;
}

EbpfMapType
jbpf_verifier_get_map_type(uint32_t platform_specific_type)
{
    uint32_t index = platform_specific_type;
    if ((index == 0) || (index >= jbpf_map_types.size())) {
        return jbpf_map_types[0];
    }
    EbpfMapType type = jbpf_map_types[index];
#ifdef __linux__
    assert(type.platform_specific_type == platform_specific_type);
#else
    type.platform_specific_type = platform_specific_type;
#endif
    return type;
}

void
jbpf_verifier_parse_maps_section(
    std::vector<EbpfMapDescriptor>& map_descriptors,
    const char* data,
    const size_t map_def_size,
    const int map_count,
    const ebpf_platform_t* platform,
    const ebpf_verifier_options_t options)
{

    // Copy map definitions from the ELF section into a local list.
    auto mapdefs = std::vector<jbpf_load_map_def>();
    for (int i = 0; i < map_count; i++) {
        jbpf_load_map_def def = {0};
        memcpy(&def, data + i * map_def_size, std::min(map_def_size, sizeof(def)));
        mapdefs.emplace_back(def);
    }

    // Add map definitions into the map_descriptors list.
    for (const auto& s : mapdefs) {
        EbpfMapType type = jbpf_verifier_get_map_type(s.type);
        map_descriptors.emplace_back(EbpfMapDescriptor{
            .original_fd = create_map_jbpf(s.type, s.key_size, s.value_size, s.max_entries, options),
            .type = s.type,
            .key_size = s.key_size,
            .value_size = s.value_size,
            .max_entries = s.max_entries,
            .inner_map_fd = s.inner_map_idx // Temporarily fill in the index. This will be replaced in the
                                            // resolve_inner_map_references pass.
        });
    }
}

// Initialize the inner_map_fd in each map descriptor.
void
resolve_inner_map_references_jbpf(std::vector<EbpfMapDescriptor>& map_descriptors)
{
    for (size_t i = 0; i < map_descriptors.size(); i++) {
        const unsigned int inner = map_descriptors[i].inner_map_fd; // Get the inner_map_idx back.
        if (inner >= map_descriptors.size()) {
            throw UnmarshalError("bad inner map index " + std::to_string(inner) + " for map " + std::to_string(i));
        }
        map_descriptors[i].inner_map_fd = map_descriptors.at(inner).original_fd;
    }
}

static int
create_map_jbpf(
    uint32_t map_type, uint32_t key_size, uint32_t value_size, uint32_t max_entries, ebpf_verifier_options_t options)
{
    if (options.mock_map_fds) {
        EbpfMapType type = jbpf_verifier_get_map_type(map_type);
        return create_map_crab(type, key_size, value_size, max_entries, options);
    } else {
        throw std::runtime_error(std::string("cannot create a Linux map"));
    }
}

EbpfMapDescriptor&
jbpf_verifier_get_map_descriptor(int map_fd)
{
    // First check if we already have the map descriptor cached.
    EbpfMapDescriptor* map = find_map_descriptor(map_fd);
    if (map != nullptr) {
        return *map;
    }

    // This fd was not created from the maps section of an ELF file,
    // but it may be an fd created by an app before calling the verifier.
    // In this case, we would like to query the map descriptor info
    // (key size, value size) from the execution context, but this is
    // not yet supported.

    throw std::runtime_error(std::string("map_fd not found"));
}

const ebpf_platform_t g_ebpf_platform_jbpf = {
    jbpf_verifier_get_program_type,
    jbpf_verifier_get_helper_prototype,
    jbpf_verifier_is_helper_usable,
    sizeof(jbpf_load_map_def),
    jbpf_verifier_parse_maps_section,
    jbpf_verifier_get_map_descriptor,
    jbpf_verifier_get_map_type,
    resolve_inner_map_references_jbpf,
    bpf_conformance_groups_t::default_groups | bpf_conformance_groups_t::packet};
