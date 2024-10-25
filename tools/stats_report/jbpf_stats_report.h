#include <stdint.h>

/* Struct definitions */
typedef struct jbpf_hook_perf
{
    uint64_t num;
    uint64_t min;
    uint64_t max;
    uint32_t hist_count;
    uint32_t hist[64];
    char hook_name[32];
} jbpf_hook_perf;

typedef struct jbpf_out_perf_list
{
    uint64_t timestamp;
    uint32_t meas_period;
    uint32_t hook_perf_count;
    jbpf_hook_perf hook_perf[32];
} jbpf_out_perf_list;