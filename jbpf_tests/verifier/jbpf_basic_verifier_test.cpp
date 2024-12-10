#include <cassert>
#include <iostream>

#include "jbpf_verifier.hpp"

std::string correct_codelets[] = {
    "/jbpf_tests/test_files/codelets/codelet-control-input/codelet-control-input.o",
    "/jbpf_tests/test_files/codelets/codelet-control-input-output/codelet-control-input.o",
    "/jbpf_tests/test_files/codelets/codelet-hashmap/codelet-hashmap.o",
    "/jbpf_tests/test_files/codelets/codelet-large-map/codelet-large-map.o",
    "/jbpf_tests/test_files/codelets/codelet-large-map-output/codelet-large-map.o",
    "/jbpf_tests/test_files/codelets/codelet-map-array/codelet-map-array.o",
    "/jbpf_tests/test_files/codelets/codelet-multithreading-output/codelet-multithreading.o",
    "/jbpf_tests/test_files/codelets/codelet-per-thread/codelet-per-thread.o",
    "/jbpf_tests/test_files/codelets/simple_input_shared/simple_input_shared.o",
    "/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
    "/jbpf_tests/test_files/codelets/time_events/time_events.o",
    "/jbpf_tests/test_files/codelets/simple_output_shared/simple_output_shared.o",
    "/jbpf_tests/test_files/codelets/simple_test_shared_multiple_maps/simple_test1_shared_multiple_maps.o",
    "/jbpf_tests/test_files/codelets/simple_test_shared_multiple_maps/simple_test2_shared_multiple_maps.o",
    "/jbpf_tests/test_files/codelets/simple_test1/simple_test1.o",
    "/jbpf_tests/test_files/codelets/simple_test2/simple_test2.o",
    "/jbpf_tests/test_files/codelets/simple_test1_shared/simple_test1_shared.o",
    "/jbpf_tests/test_files/codelets/simple_test2_shared/simple_test2_shared.o",
    "/jbpf_tests/test_files/codelets/simple_input_output/simple_input_output.o",
    "/jbpf_tests/test_files/codelets/helper-function-jbpf-hash/jbpf_helper_example.o",
    "/tools/stats_report/jbpf_stats_report.o"};

std::string error_codelets[] = {
    "/jbpf_tests/test_files/codelets/error_infinite_loop/error_infinite_loop.o",
    "/jbpf_tests/test_files/codelets/error_unknown_helper/error_unknown_helper.o",
    "/jbpf_tests/test_files/codelets/error_invalid_ctx/error_invalid_ctx.o",
};

int
main(int argc, char** argv)
{

    jbpf_verifier_result_t result;
    char* jbpf_path = getenv("JBPF_PATH");
    assert(jbpf_path != nullptr);

    // All the below codelets should pass verification
    for (auto codelet : correct_codelets) {
        std::string full_path = std::string(jbpf_path);
        full_path.append(codelet);
        result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
        std::cout << "Codelet: " << full_path << " Verification Pass: " << result.verification_pass << std::endl;
        assert(result.verification_pass);
    }

    // All the below codelets should fail verification
    for (auto codelet : error_codelets) {
        std::string full_path = std::string(jbpf_path);
        full_path.append(codelet);
        result = jbpf_verify(full_path.c_str(), nullptr, nullptr);
        assert(!result.verification_pass);
    }

    return 0;
}
