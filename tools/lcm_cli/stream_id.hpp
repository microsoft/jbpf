// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef LCM_CLI_STREAM_ID_HPP
#define LCM_CLI_STREAM_ID_HPP

#include <string>
#include <vector>

#include "jbpf_lcm_api.h"

namespace jbpf_lcm_cli {
namespace stream_id {
int
generate_from_strings(std::vector<std::string> seed, jbpf_io_stream_id_t* dest);
int
from_hex(std::string hex, jbpf_io_stream_id_t* dest);
} // namespace stream_id
} // namespace jbpf_lcm_cli

#endif // LCM_CLI_STREAM_ID_HPP
