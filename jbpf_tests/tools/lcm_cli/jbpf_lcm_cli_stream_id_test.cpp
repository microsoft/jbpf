#include <cassert>
#include <vector>

#include "stream_id.hpp"
#include "jbpf_lcm_api.h"
#include "jbpf_test_lib.h"

struct from_hex_test_case_t
{
    std::string hex;
    uint8_t stream_id[JBPF_IO_STREAM_ID_LEN];
};

from_hex_test_case_t passing_from_hex_test[] = {
    {.hex = "00112233445566778899AABBCCDDEEFF",
     .stream_id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}},
    {.hex = "000102030405060708090A0B0C0D0E0F",
     .stream_id = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}}};

std::string error_from_hex_test[] = {
    // too small
    "",
    " ",
    "00112233445566778899AABBCCDDEEF",
    // too large
    "00112233445566778899AABBCCDDEEFFF",

    // ** Below test case removed because it results in an address sanitizer issue:
    // **   stack-use-after-scope due to the usage of endptr arg in strtol
    // // invalid char
    // "00112233445566778899AABBCCDDEEGG",
};

struct generate_from_strings_test_case_t
{
    std::vector<std::string> seed;
    uint8_t stream_id[JBPF_IO_STREAM_ID_LEN];
};

generate_from_strings_test_case_t passing_generate_from_strings_test[] = {
    {.seed = {"abc", "def", "ghi"},
     .stream_id = {0xf6, 0xf5, 0xc0, 0xc7, 0x54, 0x7b, 0x19, 0xaf, 0x00, 0xa8, 0xa7, 0x54, 0xec, 0x7a, 0x55, 0xa2}},
    {.seed = {"abc", "ghi", "def"},
     .stream_id = {0x39, 0x12, 0x9a, 0xb9, 0x14, 0x51, 0x8d, 0x90, 0xcf, 0x4f, 0xfd, 0x2a, 0xac, 0x50, 0xc1, 0x9d}},
    {.seed = {"aaa", "def", "ghi"},
     .stream_id = {0x06, 0xea, 0x77, 0xde, 0x4f, 0x6e, 0x11, 0x7f, 0x00, 0xa8, 0xa7, 0x54, 0xec, 0x7a, 0x55, 0xa2}}};

std::vector<std::string> error_generate_from_strings_test[] = {
    // no seed values
    {}};

int
main(int argc, char** argv)
{
    // All the below test cases should generate a stream id
    for (auto tc : passing_from_hex_test) {
        jbpf_io_stream_id_t stream_id;
        assert(!jbpf_lcm_cli::stream_id::from_hex(tc.hex, &stream_id));

        for (int i = 0; i < JBPF_IO_STREAM_ID_LEN; i++)
            assert(stream_id.id[i] == tc.stream_id[i]);
    }

    // All the below test cases should fail to generate a stream id
    for (auto hex : error_from_hex_test) {
        jbpf_io_stream_id_t stream_id;
        __assert__(jbpf_lcm_cli::stream_id::from_hex(hex, &stream_id));
    }

    // All the below test cases should generate a stream id
    for (auto tc : passing_generate_from_strings_test) {
        jbpf_io_stream_id_t stream_id;
        assert(!jbpf_lcm_cli::stream_id::generate_from_strings(tc.seed, &stream_id));

        for (int i = 0; i < JBPF_IO_STREAM_ID_LEN; i++)
            assert(stream_id.id[i] == tc.stream_id[i]);
    }

    // All the below test cases should fail to generate a stream id
    for (auto seed : error_generate_from_strings_test) {
        jbpf_io_stream_id_t stream_id;
        __assert__(jbpf_lcm_cli::stream_id::generate_from_strings(seed, &stream_id));
    }

    return 0;
}