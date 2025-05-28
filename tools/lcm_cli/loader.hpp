// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef LOADER_HPP
#define LOADER_HPP

#include <vector>
#include <string>

namespace jbpf_lcm_cli {
namespace loader {
enum load_req_outcome
{
    JBPF_LCM_CLI_REQ_SUCCESS,
    JBPF_LCM_CLI_INVALID_ARGS,
    JBPF_LCM_CLI_VERIFIER_FAILED,
    JBPF_LCM_CLI_PARSE_FAILED,
    JBPF_LCM_CLI_REQ_FAILURE,
    JBPF_LCM_CLI_EXIT
};

load_req_outcome
run_loader(int ac, char** av);

load_req_outcome
run_lcm_subproc(const std::vector<std::string>& str_args, const std::string& inline_bin_arg);
} // namespace loader
} // namespace jbpf_lcm_cli

#endif // LOADER_HPP
