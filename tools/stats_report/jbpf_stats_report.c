#include "jbpf_defs.h"
#include "jbpf_helper.h"
#include "jbpf_perf_ext.h"
#include "jbpf_stats_report.h"
#include <string.h>

#define NUM_HIST_BINS 64

/* Code snippet to count __VA_ARGS__: https://gist.github.com/aprell/3722962*/
#define VA_NARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(_, ##__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define DEFINE_STATS_HOOKS(...)                                              \
    static const char hook_names[VA_NARGS(__VA_ARGS__)][32] = {__VA_ARGS__}; \
    int num_hooks = VA_NARGS(__VA_ARGS__);

jbpf_ringbuf_map(output_map, jbpf_out_perf_list, 10);

struct jbpf_load_map_def SEC("maps") stats_tmp = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(jbpf_out_perf_list),
    .max_entries = 1,
};

SEC("jbpf_stats")
uint64_t
jbpf_main(void* state)
{

    struct jbpf_stats_ctx* ctx;
    struct jbpf_perf_hook_list *hook_list, *hook_list_end;
    jbpf_out_perf_list* out_hook_list;
    uint64_t index = 0;
    uint16_t total = 0;

    ctx = (struct jbpf_stats_ctx*)state;

    out_hook_list = jbpf_map_lookup_reset_elem(&stats_tmp, &index);

    if (!out_hook_list)
        return 1;

    out_hook_list->meas_period = ctx->meas_period;
    out_hook_list->timestamp = jbpf_time_get_ns();

    hook_list = (struct jbpf_perf_hook_list*)ctx->data;
    hook_list_end = (struct jbpf_perf_hook_list*)ctx->data_end;

    if (hook_list + 1 > hook_list_end)
        return 1;

    out_hook_list->hook_perf_count = 0;

    // DEFINE_STATS_HOOKS("test1", "report_stats", "test2", "test4")

    //#pragma unroll
    // for (int j = 0; j < num_hooks; j++)
    {

        for (volatile int i = 0; i < hook_list->num_reported_hooks; i++) {

            if (hook_list->perf_data[i & 63].num == 0)
                continue;
            // if (!___same(hook_names[j & 3], hook_list->perf_data[i & 63].hook_name, 32))
            //   continue;

            out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].max = hook_list->perf_data[i & 63].max;
            out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].min = hook_list->perf_data[i & 63].min;
            out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].num = hook_list->perf_data[i & 63].num;

            out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].hist_count = NUM_HIST_BINS;
            memcpy(
                out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].hist,
                hook_list->perf_data[i & 63].hist,
                NUM_HIST_BINS * sizeof(uint32_t));
            memcpy(
                out_hook_list->hook_perf[out_hook_list->hook_perf_count & 31].hook_name,
                hook_list->perf_data[i & 63].hook_name,
                32);
            out_hook_list->hook_perf_count++;
            total++;
        }
    }

    if (total) {
        int attempts = 0;
        int ret = -1;
        while (ret != 0) {
            ret = jbpf_ringbuf_output(&output_map, out_hook_list, sizeof(jbpf_out_perf_list));
            if (++attempts == 3)
                return 1;
        }
    }

    return 0;
}
