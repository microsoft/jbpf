/*
 * The purpose of this test is to check the correct functionality of the perf API.
 *
 * This test does the following:
 * 1. It measures the elapsed time using the perf API with both valid and invalid time values.
 * 2. It checks that the perf API correctly handles invalid time values and does not crash.
 */

#include <assert.h>

#include "jbpf.h"
#include "jbpf_perf.h"

#include "jbpf_logging.h"

int
main(int argc, char* argv[])
{

    struct jbpf_config config = {0};
    uint32_t perf_idx, min_time, max_time;
    struct jbpf_hook test_hook = {
        .name = "test_hook",
        .hook_type = JBPF_HOOK_TYPE_MON,
        .jbpf_perf_active = JBPF_HOOK_PERF_DEFAULT_STATE,
    };

    jbpf_set_default_config_options(&config);

    assert(jbpf_init(&config) == 0);

    jbpf_init_perf_hook(&test_hook);

    // The thread will be calling jbpf perf functions, so we need to register it
    jbpf_register_thread();

    perf_idx = get_jbpf_hook_thread_id();
    assert(perf_idx > 0 && perf_idx < JBPF_MAX_NUM_REG_THREADS); // Ensure the perf index is within bounds

    // Start the perf measurement with a valid time
    uint64_t start_time = jbpf_measure_start_time();
    // Simulate some processing
    usleep(1000); // Sleep for 1ms to simulate processing time
    uint64_t end_time = jbpf_measure_stop_time();

    // Log the valid time measurement
    struct jbpf_perf_data* perf_data = jbpf_get_perf_data(&test_hook);
    assert(perf_data != NULL); // Ensure perf_data is not NULL

    int res = _jbpf_perf_log(perf_data, start_time, end_time);
    assert(res == 0); // Ensure the logging was successful

    // Check that the valid time measurement was logged correctly
    assert(perf_data[perf_idx].num > 0);
    min_time = perf_data[perf_idx].min;
    max_time = perf_data[perf_idx].max;
    assert(min_time > 0); // Minimum time should be greater than 0
    assert(max_time > 0); // Maximum time should be greater than 0
    for (int i = 0; i < JBPF_NUM_HIST_BINS; i++) {
        assert(perf_data[perf_idx].hist[i] >= 0); // Histogram bins should not be negative
    }

    // Now, let's test with an invalid time value
    uint64_t invalid_start_time = 12345; // Invalid start time
    uint64_t invalid_end_time = 12345;   // Invalid end time
    // Attempt to log the invalid time measurement
    res = _jbpf_perf_log(perf_data, invalid_start_time, invalid_end_time);
    // Ensure that the logging of invalid time does not crash and returns an error
    assert(res == -1); // Expecting an error for invalid time
    // Check that the perf data remains unchanged after invalid logging
    assert(perf_data[perf_idx].num == 1);        // Should still have the valid measurement logged
    assert(perf_data[perf_idx].min == min_time); // Minimum should still be the same
    assert(perf_data[perf_idx].max == max_time); // Maximum should still be the same

    jbpf_free_perf_hook(&test_hook);

    // Stop
    jbpf_stop();
}