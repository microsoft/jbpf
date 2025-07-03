// Copyright (c) Microsoft Corporation. All rights reserved.

#include "parser.hpp"

#include <iostream>
#include <boost/foreach.hpp>
#include <iomanip>
#include <regex>
#include "stream_id.hpp"
#include "jbpf_verifier.hpp"

namespace jbpf_reverse_proxy {
namespace parser {
using namespace std;
using boost::property_tree::ptree;

// verifier function
jbpf_verify_func_t parser_jbpf_verifier_func = NULL;

void
parser_jbpf_set_verify_func(jbpf_verify_func_t func)
{
    parser_jbpf_verifier_func = func;
}

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
parse_jbpf_io_channel_desc(const ptree pt, const string path, jbpf_io_channel_desc_s* dest, vector<string> stream_elems)
{
    auto name = pt.get_child("name").get_value<string>();
    if (name.length() > JBPF_IO_CHANNEL_NAME_LEN - 1) {
        cout << "codelet_descriptor[]." << path << "[].name length must be at most " << JBPF_IO_CHANNEL_NAME_LEN - 1
             << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    name.copy(dest->name, JBPF_IO_CHANNEL_NAME_LEN - 1);
    dest->name[name.length()] = '\0';

    auto stream_id = pt.get_child_optional("stream_id");
    if (stream_id) {
        if (jbpf_lcm_cli::stream_id::from_hex(stream_id.value().get_value<string>(), &dest->stream_id))
            return JBPF_LCM_PARSE_REQ_FAILED;
    } else {
        vector<string> map_elems;

        for (auto elem : stream_elems)
            map_elems.push_back(elem);

        map_elems.push_back(name);

        if (jbpf_lcm_cli::stream_id::generate_from_strings(map_elems, &dest->stream_id))
            return JBPF_LCM_PARSE_REQ_FAILED;
    }
    auto oFilePathNode = pt.get_child_optional("serde.file_path");
    if (oFilePathNode) {
        auto file_path = oFilePathNode.get().get_value<string>();
        auto_expand_environment_variables(file_path);
        if (file_path.length() > JBPF_PATH_LEN - 1) {
            cout << "codelet_descriptor[]." << path << "[].serde.file_path length must be at most " << JBPF_PATH_LEN - 1
                 << endl;
            return JBPF_LCM_PARSE_REQ_FAILED;
        }
        file_path.copy(dest->serde.file_path, JBPF_PATH_LEN - 1);
        dest->serde.file_path[file_path.length()] = '\0';
        dest->has_serde = true;
    }
    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_linked_map_descriptor(const ptree pt, jbpf_linked_map_descriptor_s* dest)
{
    auto const linked_codelet_name = pt.get_child("linked_codelet_name").get_value<string>();
    if (linked_codelet_name.length() > JBPF_CODELET_NAME_LEN - 1) {
        cout << "codelet_descriptor[].linked_maps[].linked_codelet_name length must be at most "
             << JBPF_CODELET_NAME_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    linked_codelet_name.copy(dest->linked_codelet_name, JBPF_CODELET_NAME_LEN - 1);
    dest->linked_codelet_name[linked_codelet_name.length()] = '\0';

    auto linked_map_name = pt.get_child("linked_map_name").get_value<string>();
    if (linked_map_name.length() > JBPF_MAP_NAME_LEN - 1) {
        cout << "codelet_descriptor[].linked_maps[].linked_map_name length must be at most " << JBPF_MAP_NAME_LEN - 1
             << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    linked_map_name.copy(dest->linked_map_name, JBPF_MAP_NAME_LEN - 1);
    dest->linked_map_name[linked_map_name.length()] = '\0';

    auto map_name = pt.get_child("map_name").get_value<string>();
    if (map_name.length() > JBPF_MAP_NAME_LEN - 1) {
        cout << "codelet_descriptor[].linked_maps[].map_name length must be at most " << JBPF_MAP_NAME_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    map_name.copy(dest->map_name, JBPF_MAP_NAME_LEN - 1);
    dest->map_name[map_name.length()] = '\0';

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_codelet_descriptor(const ptree pt, jbpf_codelet_descriptor_s* dest, vector<string> codelet_elems)
{
    auto codelet_name_opt = pt.get_child_optional("codelet_name");
    if (!codelet_name_opt) {
        cout << "Missing required field: codelet_name\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    auto codelet_name = codelet_name_opt->get_value<string>();
    if (codelet_name.length() > JBPF_CODELET_NAME_LEN - 1) {
        cout << "codelet_descriptor[].codelet_name length must be at most " << JBPF_CODELET_NAME_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    codelet_name.copy(dest->codelet_name, JBPF_CODELET_NAME_LEN - 1);
    dest->codelet_name[codelet_name.length()] = '\0';

    auto hook_name_opt = pt.get_child_optional("hook_name");
    if (!hook_name_opt) {
        cout << "Missing required field: hook_name\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    auto hook_name = hook_name_opt->get_value<string>();
    if (hook_name.length() > JBPF_HOOK_NAME_LEN - 1) {
        cout << "codelet_descriptor[].hook_name length must be at most " << JBPF_HOOK_NAME_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    hook_name.copy(dest->hook_name, JBPF_HOOK_NAME_LEN - 1);
    dest->hook_name[hook_name.length()] = '\0';

    auto codelet_path_opt = pt.get_child_optional("codelet_path");
    if (!codelet_path_opt) {
        cout << "Missing required field: codelet_path\n";
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    auto codelet_path = codelet_path_opt->get_value<string>();
    auto_expand_environment_variables(codelet_path);
    if (codelet_path.length() > JBPF_PATH_LEN - 1) {
        cout << "codelet_descriptor[].codelet_path length must be at most " << JBPF_PATH_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_FAILED;
    }
    codelet_path.copy(dest->codelet_path, JBPF_PATH_LEN - 1);
    dest->codelet_path[codelet_path.length()] = '\0';

    if (parser_jbpf_verifier_func == NULL) {
        cout << "Verifier function not set" << endl;
        return JBPF_LCM_PARSE_VERIFIER_FAILED;
    }

    auto result = parser_jbpf_verifier_func(codelet_path.c_str(), nullptr, nullptr);
    if (!result.verification_pass) {
        cout << "Codelet verification failed: " << result.err_msg << endl;
        return JBPF_LCM_PARSE_VERIFIER_FAILED;
    }

    auto priority = pt.get_child_optional("priority");
    dest->priority = priority ? priority.get().get_value<uint32_t>() : JBPF_CODELET_PRIORITY_DEFAULT;

    auto runtime_threshold = pt.get_child_optional("runtime_threshold");
    if (runtime_threshold)
        dest->runtime_threshold = runtime_threshold.get().get_value<uint32_t>();

    auto in_io_channel = pt.get_child_optional("in_io_channel");
    if (in_io_channel) {
        auto idx = 0;
        BOOST_FOREACH (const ptree::value_type& child, in_io_channel.value()) {
            if (idx >= JBPF_MAX_IO_CHANNEL) {
                cout << "Too many in_io_channel entries (max " << JBPF_MAX_IO_CHANNEL << ")\n";
                return JBPF_LCM_PARSE_REQ_FAILED;
            }
            vector<string> stream_elems = codelet_elems;
            stream_elems.push_back(codelet_name);
            stream_elems.push_back(hook_name);
            stream_elems.push_back("input");
            auto ret =
                parse_jbpf_io_channel_desc(child.second, "in_io_channel", &dest->in_io_channel[idx], stream_elems);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS)
                return ret;
            idx++;
        }
        dest->num_in_io_channel = idx;
    }

    auto out_io_channel = pt.get_child_optional("out_io_channel");
    if (out_io_channel) {
        auto idx = 0;
        BOOST_FOREACH (const ptree::value_type& child, out_io_channel.value()) {
            if (idx >= JBPF_MAX_IO_CHANNEL) {
                cout << "Too many out_io_channel entries (max " << JBPF_MAX_IO_CHANNEL << ")\n";
                return JBPF_LCM_PARSE_REQ_FAILED;
            }
            vector<string> stream_elems = codelet_elems;
            stream_elems.push_back(codelet_name);
            stream_elems.push_back(hook_name);
            stream_elems.push_back("output");
            auto ret =
                parse_jbpf_io_channel_desc(child.second, "out_io_channel", &dest->out_io_channel[idx], stream_elems);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS)
                return ret;
            idx++;
        }
        dest->num_out_io_channel = idx;
    }

    auto linked_maps = pt.get_child_optional("linked_maps");
    if (linked_maps) {
        auto idx = 0;
        BOOST_FOREACH (const ptree::value_type& child, linked_maps.value()) {
            if (idx >= JBPF_MAX_LINKED_MAPS) {
                cout << "Too many linked_maps entries (max " << JBPF_MAX_LINKED_MAPS << ")\n";
                return JBPF_LCM_PARSE_REQ_FAILED;
            }
            auto ret = parse_jbpf_linked_map_descriptor(child.second, &dest->linked_maps[idx]);
            if (ret != JBPF_LCM_PARSE_REQ_SUCCESS)
                return ret;
            idx++;
        }
        dest->num_linked_maps = idx;
    }

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}
} // namespace internal

jbpf_verify_func_t verifier_func = NULL;

parse_req_outcome
parse_jbpf_codeletset_load_req(const ptree pt, jbpf_codeletset_load_req* dest, vector<string> codeletset_elems)
{
    auto name = pt.get_child("codeletset_id").get_value<string>();
    if (name.length() > JBPF_CODELETSET_NAME_LEN - 1) {
        cout << "codeletset_id length must be at most " << JBPF_CODELETSET_NAME_LEN - 1 << endl;
        return JBPF_LCM_PARSE_REQ_SUCCESS;
    }
    name.copy(dest->codeletset_id.name, JBPF_CODELETSET_NAME_LEN - 1);
    dest->codeletset_id.name[name.length()] = '\0';

    auto idx = 0;
    BOOST_FOREACH (const ptree::value_type& child, pt.get_child("codelet_descriptor")) {
        vector<string> codelet_elems;
        for (auto elem : codeletset_elems)
            codelet_elems.push_back(elem);
        codelet_elems.push_back(name);
        auto ret = internal::parse_jbpf_codelet_descriptor(child.second, &dest->codelet_descriptor[idx], codelet_elems);
        if (ret != JBPF_LCM_PARSE_REQ_SUCCESS)
            return ret;
        idx++;
    }
    dest->num_codelet_descriptors = idx;

    return JBPF_LCM_PARSE_REQ_SUCCESS;
}

parse_req_outcome
parse_jbpf_codeletset_unload_req(const ptree pt, jbpf_codeletset_unload_req* dest)
{
    auto name = pt.get_child("codeletset_id").get_value<string>();
    if (name.length() > JBPF_CODELETSET_NAME_LEN - 1) {
        cout << "codeletset_id length must be at most " << JBPF_CODELETSET_NAME_LEN - 1 << endl;
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
} // namespace jbpf_reverse_proxy