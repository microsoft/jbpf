// Copyright (c) Microsoft Corporation. All rights reserved.

#include "parser.hpp"
#include "yaml-cpp/yaml.h"

#include <iostream>
#include <iomanip>
#include <regex>
#include "stream_id.hpp"

namespace jbpf_lcm_cli {
namespace parser {
using namespace std;

namespace internal {
void
auto_expand_environment_variables(string& text)
{
    static regex env("\\$\\{([^}]+)\\}");
    smatch match;
    while (regex_search(text, match, env)) {
        const char* s = getenv(match[1].str().c_str());
        const string var(s == NULL ? "" : s);
        text.replace(match[0].first, match[0].second, var);
    }
}

parse_req_outcome
parse_jbpf_io_channel_desc(
    YAML::Node cfg, const string path, jbpf_io_channel_desc_s* dest, const vector<string>& stream_elems)
{
    if (!cfg.IsDefined() || !cfg["name"].IsDefined())
        return JBPF_LCM_PARSE_REQ_FAILED;
    auto name = cfg["name"].as<string>();
    if (name.length() > JBPF_IO_CHANNEL_NAME_LEN - 1 || name.empty()) {
        cout << "codelet_descriptor[]." << path << "[].name length must be between 1 and "
             << JBPF_IO_CHANNEL_NAME_LEN - 1 << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    name.copy(dest->name, JBPF_IO_CHANNEL_NAME_LEN - 1);
    dest->name[name.length()] = '\0';

    if (cfg["stream_id"].IsDefined()) {
        auto stream_id = cfg["stream_id"].as<string>();
        if (jbpf_lcm_cli::stream_id::from_hex(stream_id, &dest->stream_id))
            return JBPF_LCM_PARSE_REQ_FAILED;
    } else {
        vector<string> map_elems = stream_elems;
        map_elems.emplace_back(name);

        if (jbpf_lcm_cli::stream_id::generate_from_strings(map_elems, &dest->stream_id))
            return JBPF_LCM_PARSE_REQ_FAILED;
    }

    if (cfg["serde"].IsDefined() && cfg["serde"]["file_path"].IsDefined()) {
        auto file_path = cfg["serde"]["file_path"].as<string>();
        auto_expand_environment_variables(file_path);
        if (file_path.length() > JBPF_PATH_LEN - 1 || name.empty()) {
            cout << "codelet_descriptor[]." << path << "[].serde.file_path length must be between 1 and "
                 << JBPF_PATH_LEN - 1 << " characters long." << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }

        file_path.copy(dest->serde.file_path, JBPF_PATH_LEN - 1);
        dest->serde.file_path[file_path.length()] = '\0';
        dest->has_serde = true;
    }
    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_linked_map_descriptor(YAML::Node cfg, jbpf_linked_map_descriptor_s* dest)
{
    if (!cfg.IsDefined() || !cfg["linked_codelet_name"].IsDefined() || !cfg["linked_map_name"].IsDefined() ||
        !cfg["map_name"].IsDefined())
        return JBPF_LCM_PARSE_REQ_FAILED;

    auto linked_codelet_name = cfg["linked_codelet_name"].as<string>();
    if (linked_codelet_name.length() > JBPF_CODELET_NAME_LEN - 1 || linked_codelet_name.empty()) {
        cout << "codelet_descriptor[].linked_maps[].linked_codelet_name length must be between 1 and "
             << JBPF_CODELET_NAME_LEN - 1 << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    linked_codelet_name.copy(dest->linked_codelet_name, JBPF_CODELET_NAME_LEN - 1);
    dest->linked_codelet_name[linked_codelet_name.length()] = '\0';

    auto linked_map_name = cfg["linked_map_name"].as<string>();
    if (linked_map_name.length() > JBPF_MAP_NAME_LEN - 1 || linked_map_name.empty()) {
        cout << "codelet_descriptor[].linked_maps[].linked_map_name length must be between 1 and "
             << JBPF_MAP_NAME_LEN - 1 << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    linked_map_name.copy(dest->linked_map_name, JBPF_MAP_NAME_LEN - 1);
    dest->linked_map_name[linked_map_name.length()] = '\0';

    auto map_name = cfg["map_name"].as<string>();
    if (map_name.length() > JBPF_MAP_NAME_LEN - 1 || map_name.empty()) {
        cout << "codelet_descriptor[].linked_maps[].map_name length must be between 1 and " << JBPF_MAP_NAME_LEN - 1
             << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    map_name.copy(dest->map_name, JBPF_MAP_NAME_LEN - 1);
    dest->map_name[map_name.length()] = '\0';

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_codelet_descriptor(YAML::Node cfg, jbpf_codelet_descriptor_s* dest, const vector<string>& codelet_elems)
{
    if (!cfg.IsDefined() || !cfg["codelet_name"].IsDefined() || !cfg["hook_name"].IsDefined() ||
        !cfg["codelet_path"].IsDefined()) {
        cout << "Missing required fields: codelet_name, hook_name, or codelet_path\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    auto codelet_name = cfg["codelet_name"].as<string>();
    if (codelet_name.length() > JBPF_CODELET_NAME_LEN - 1) {
        cout << "codelet_descriptor[].codelet_name length must between 1 and " << JBPF_CODELET_NAME_LEN - 1
             << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    codelet_name.copy(dest->codelet_name, JBPF_CODELET_NAME_LEN - 1);
    dest->codelet_name[codelet_name.length()] = '\0';

    auto hook_name = cfg["hook_name"].as<string>();
    if (hook_name.length() > JBPF_HOOK_NAME_LEN - 1) {
        cout << "codelet_descriptor[].hook_name length must between 1 and " << JBPF_HOOK_NAME_LEN - 1
             << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    hook_name.copy(dest->hook_name, JBPF_HOOK_NAME_LEN - 1);
    dest->hook_name[hook_name.length()] = '\0';

    auto codelet_path = cfg["codelet_path"].as<string>();
    auto_expand_environment_variables(codelet_path);
    if (codelet_path.length() > JBPF_PATH_LEN - 1) {
        cout << "codelet_descriptor[].codelet_path length must between 1 and " << JBPF_PATH_LEN - 1
             << " characters long." << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    codelet_path.copy(dest->codelet_path, JBPF_PATH_LEN - 1);
    dest->codelet_path[codelet_path.length()] = '\0';

    if (cfg["priority"].IsDefined()) {
        auto priority = cfg["priority"].as<uint32_t>();
        dest->priority = priority;
    } else {
        dest->priority = JBPF_CODELET_PRIORITY_DEFAULT;
    }

    if (cfg["runtime_threshold"].IsDefined()) {
        auto runtime_threshold = cfg["runtime_threshold"].as<uint32_t>();
        dest->runtime_threshold = runtime_threshold;
    }

    if (cfg["in_io_channel"].IsDefined()) {
        if (!cfg["in_io_channel"].IsSequence()) {
            cout << "codelet_descriptor[].in_io_channel must be a sequence" << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }

        dest->num_in_io_channel = cfg["in_io_channel"].size();
        if (dest->num_in_io_channel > JBPF_MAX_IO_CHANNEL) {
            cout << "codelet_descriptor[].in_io_channel exceeds maximum number of input channels: "
                 << JBPF_MAX_IO_CHANNEL << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }
        vector<string> stream_elems_in = codelet_elems;
        stream_elems_in.emplace_back(codelet_name);
        stream_elems_in.emplace_back(hook_name);
        stream_elems_in.emplace_back("input");
        for (int idx = 0; idx < dest->num_in_io_channel; idx++) {
            auto ret = parse_jbpf_io_channel_desc(
                cfg["in_io_channel"][idx], "in_io_channel", &dest->in_io_channel[idx], stream_elems_in);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS) {
                return ret;
            }
        }
    }

    if (cfg["out_io_channel"].IsDefined()) {
        if (!cfg["out_io_channel"].IsSequence()) {
            cout << "codelet_descriptor[].out_io_channel must be a sequence" << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }

        dest->num_out_io_channel = cfg["out_io_channel"].size();
        if (dest->num_out_io_channel > JBPF_MAX_IO_CHANNEL) {
            cout << "codelet_descriptor[].out_io_channel exceeds maximum number of output channels: "
                 << JBPF_MAX_IO_CHANNEL << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }
        vector<string> stream_elems_out = codelet_elems;
        stream_elems_out.emplace_back(codelet_name);
        stream_elems_out.emplace_back(hook_name);
        stream_elems_out.emplace_back("output");
        for (int idx = 0; idx < dest->num_out_io_channel; idx++) {
            auto ret = parse_jbpf_io_channel_desc(
                cfg["out_io_channel"][idx], "out_io_channel", &dest->out_io_channel[idx], stream_elems_out);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS)
                return ret;
        }
    }

    if (cfg["linked_maps"].IsDefined()) {
        if (!cfg["linked_maps"].IsSequence()) {
            cout << "codelet_descriptor[].linked_maps must be a sequence" << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }

        dest->num_linked_maps = cfg["linked_maps"].size();
        for (int idx = 0; idx < dest->num_linked_maps; idx++) {
            auto ret = parse_jbpf_linked_map_descriptor(cfg["linked_maps"][idx], &dest->linked_maps[idx]);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS) {
                return ret;
            }
        }
    }

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}
} // namespace internal

parse_req_outcome
parse_jbpf_codeletset_load_req(YAML::Node cfg, jbpf_codeletset_load_req* dest, const vector<string>& codeletset_elems)
{
    // Validate presence and types of required YAML fields
    if (!cfg.IsDefined() || !cfg["codeletset_id"].IsDefined() || !cfg["codelet_descriptor"].IsDefined() ||
        !cfg["codelet_descriptor"].IsSequence()) {
        cout << "Missing or invalid 'codeletset_id' or 'codelet_descriptor' fields in YAML.\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    // Parse and validate codeletset_id
    auto name = cfg["codeletset_id"].as<std::string>();
    if (name.empty() || name.length() > JBPF_CODELETSET_NAME_LEN - 1) {
        cout << "codeletset_id length must be between 1 and " << JBPF_CODELETSET_NAME_LEN - 1 << " characters long.\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    name.copy(dest->codeletset_id.name, JBPF_CODELETSET_NAME_LEN - 1);
    dest->codeletset_id.name[name.length()] = '\0';

    // Validate number of descriptors
    std::size_t num_descriptors = cfg["codelet_descriptor"].size();
    if (num_descriptors > JBPF_MAX_CODELETS_IN_CODELETSET) {
        cout << "Too many codelet descriptors: max allowed is " << JBPF_MAX_CODELETS_IN_CODELETSET << "\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    dest->num_codelet_descriptors = static_cast<int>(num_descriptors);
    if (dest->num_codelet_descriptors == 0) {
        cout << "No codelet descriptors provided.\n";
    }
    std::vector<std::string> codelet_elems = codeletset_elems;
    codelet_elems.emplace_back(name);
    for (size_t i = 0; i < dest->num_codelet_descriptors; i++) {
        auto ret = internal::parse_jbpf_codelet_descriptor(
            cfg["codelet_descriptor"][i], &dest->codelet_descriptor[i], codelet_elems);

        if (ret != JBPF_LCM_PARSE_REQ_SUCCESS) {
            cout << "Failed to parse codelet_descriptor at index " << i << "\n";
            return ret;
        }
    }

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_codeletset_unload_req(YAML::Node cfg, jbpf_codeletset_unload_req* dest)
{
    if (!cfg.IsDefined() || !cfg["codeletset_id"].IsDefined()) {
        cout << "Missing required 'codeletset_id' field in YAML config.\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    std::string name = cfg["codeletset_id"].as<std::string>();

    if (name.empty()) {
        cout << "codeletset_id cannot be empty.\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    if (name.length() > JBPF_CODELETSET_NAME_LEN - 1) {
        cout << "codeletset_id exceeds maximum length of " << JBPF_CODELETSET_NAME_LEN - 1 << " characters.\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }

    name.copy(dest->codeletset_id.name, JBPF_CODELETSET_NAME_LEN - 1);
    dest->codeletset_id.name[name.length()] = '\0';

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

std::string
expand_environment_variables(const std::string& input)
{
    std::string text = input;
    internal::auto_expand_environment_variables(text);
    return text;
}
} // namespace parser
} // namespace jbpf_lcm_cli