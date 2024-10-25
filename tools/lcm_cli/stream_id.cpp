// Copyright (c) Microsoft Corporation. All rights reserved.

#include <iostream>
#include <numeric>

#include "stream_id.hpp"

namespace jbpf_lcm_cli {
namespace stream_id {
using namespace std;

/**
 * @brief create deterministic stream id using simple 64 bit hash function
 *
 * @param seed text values used as seeds
 * @param dest output stream id
 * @return int 0 for success, else failure
 */
int
generate_from_strings(vector<string> seed, jbpf_io_stream_id_t* dest)
{
    if (seed.size() == 0) {
        cerr << "No seed values" << endl;
        return EXIT_FAILURE;
    }

    std::hash<std::string> hasher;
    std::size_t acc1 = 0;
    std::size_t acc2 = 0;
    int c = 0;
    for (auto& s : seed) {
        if (c % 2 == 0) {
            acc1 ^= hasher(s);
        } else {
            acc2 ^= hasher(s);
        }
        c++;
    }

    for (int i = 0; i < 8; i++) {
        dest->id[i] = static_cast<uint8_t>((acc1 >> (8 * i)) & 0xFF);
        dest->id[i + 8] = static_cast<uint8_t>((acc2 >> (8 * i)) & 0xFF);
    }

    return EXIT_SUCCESS;
}

/**
 * @brief create stream id from hexidecimal string
 *
 * @param hex input hex string
 * @param dest output stream id
 * @return int 0 for success, else failure
 */
int
from_hex(string hex, jbpf_io_stream_id_t* dest)
{
    vector<char> stream_id;

    if (hex.length() != JBPF_IO_STREAM_ID_LEN * 2) {
        cerr << "Invalid hex string length" << endl;
        return EXIT_FAILURE;
    }

    for (unsigned int i = 0; i < hex.length(); i += 2)
        stream_id.push_back((char)strtol(hex.substr(i, 2).c_str(), NULL, 16));

    copy(begin(stream_id), end(stream_id), begin(dest->id));

    return EXIT_SUCCESS;
}
} // namespace stream_id
} // namespace jbpf_lcm_cli
