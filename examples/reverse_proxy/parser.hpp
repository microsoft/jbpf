// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <sys/stat.h>
#include <boost/property_tree/ptree.hpp>
#include "jbpf_lcm_api.h"
#include "jbpf_verifier.hpp"

namespace jbpf_reverse_proxy {
namespace parser {
enum parse_req_outcome
{
    JBPF_LCM_PARSE_REQ_SUCCESS,
    JBPF_LCM_PARSE_VERIFIER_FAILED,
    JBPF_LCM_PARSE_REQ_FAILED,
};

void
parser_jbpf_set_verify_func(jbpf_verify_func_t func);

parse_req_outcome
parse_jbpf_codeletset_load_req(
    const boost::property_tree::ptree pt, jbpf_codeletset_load_req* dest, std::vector<std::string> codeletset_elems);
parse_req_outcome
parse_jbpf_codeletset_unload_req(const boost::property_tree::ptree pt, jbpf_codeletset_unload_req* dest);
std::string
expand_environment_variables(const std::string& input);
} // namespace parser
} // namespace jbpf_reverse_proxy

#endif // PARSER_HPP
