// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef PARSER_HPP
#define PARSER_HPP

#include "yaml-cpp/yaml.h"
#include <string>
#include <vector>
#include <sys/stat.h>
#include "jbpf_lcm_api.h"

namespace jbpf_lcm_cli {
namespace parser {
enum parse_req_outcome
{
    JBPF_LCM_PARSE_REQ_SUCCESS,
    JBPF_LCM_PARSE_VERIFIER_FAILED,
    JBPF_LCM_PARSE_REQ_FAILED,
};

parse_req_outcome
parse_jbpf_codeletset_load_req(
    YAML::Node cfg, jbpf_codeletset_load_req* dest, std::vector<std::string> codeletset_elems);
parse_req_outcome
parse_jbpf_codeletset_unload_req(YAML::Node cfg, jbpf_codeletset_unload_req* dest);
std::string
expand_environment_variables(const std::string& input);
} // namespace parser
} // namespace jbpf_lcm_cli

#endif // PARSER_HPP
