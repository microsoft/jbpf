#include <cassert>
#include <sstream>
#include "yaml-cpp/yaml.h"

#include "parser.hpp"
#include "jbpf_lcm_api.h"

void
test_parse_jbpf_codeletset_load_req()
{
    std::stringstream ss;
    ss << "codelet_descriptor:\n"
          "  - codelet_name: simple_output\n"
          "    codelet_path: ${JBPF_PATH}/jbpf_tests/test_files/codelets/simple_output/simple_output.o\n"
          "    hook_name: test1\n"
          "    out_io_channel:\n"
          "      - name: output_map\n"
          "        stream_id: 00112233445566778899AABBCCDDEEFF\n"
          "codeletset_id: simple_output_codeletset\n";
    auto cfg = YAML::Load(ss.str());
    jbpf_codeletset_load_req dest;
    std::vector<std::string> codeletset_elems;

    auto ret = jbpf_lcm_cli::parser::parse_jbpf_codeletset_load_req(cfg, &dest, codeletset_elems);

    assert(ret == jbpf_lcm_cli::parser::JBPF_LCM_PARSE_REQ_SUCCESS);
    std::string codeletset_id(dest.codeletset_id.name);
    assert(codeletset_id == "simple_output_codeletset");
    assert(dest.num_codelet_descriptors == 1);
    std::string codelet_name(dest.codelet_descriptor[0].codelet_name);
    assert(codelet_name == "simple_output");
    std::string hook_name(dest.codelet_descriptor[0].hook_name);
    assert(hook_name == "test1");
    std::string codelet_path(dest.codelet_descriptor[0].codelet_path);
    assert(
        codelet_path ==
        std::string(getenv("JBPF_PATH")) + "/jbpf_tests/test_files/codelets/simple_output/simple_output.o");
    assert(dest.codelet_descriptor[0].num_in_io_channel == 0);
    assert(dest.codelet_descriptor[0].num_out_io_channel == 1);
    assert(dest.codelet_descriptor[0].num_linked_maps == 0);

    std::string out_map_name(dest.codelet_descriptor[0].out_io_channel[0].name);
    assert(out_map_name == "output_map");
    assert(!dest.codelet_descriptor[0].out_io_channel[0].has_serde);

    const uint8_t test_stream_id[JBPF_IO_STREAM_ID_LEN] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    for (int i = 0; i < JBPF_IO_STREAM_ID_LEN; i++)
        assert(dest.codelet_descriptor[0].out_io_channel[0].stream_id.id[i] == (test_stream_id[i]));
}

void
test_parse_jbpf_codeletset_unload_req()
{
    std::stringstream ss;
    ss << "codeletset_id: simple_output_codeletset\n";
    auto cfg = YAML::Load(ss.str());

    jbpf_codeletset_unload_req dest;
    auto ret = jbpf_lcm_cli::parser::parse_jbpf_codeletset_unload_req(cfg, &dest);

    assert(ret == jbpf_lcm_cli::parser::JBPF_LCM_PARSE_REQ_SUCCESS);
    std::string codeletset_id(dest.codeletset_id.name);
    assert(codeletset_id == "simple_output_codeletset");
}

int
main(int argc, char** argv)
{
    test_parse_jbpf_codeletset_load_req();
    test_parse_jbpf_codeletset_unload_req();

    return 0;
}
