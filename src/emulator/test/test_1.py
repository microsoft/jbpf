import os, sys, time, ctypes

JBPF_PATH = os.getenv("JBPF_PATH")
sys.path.append(JBPF_PATH + "/out/lib/")

## import the one directory up
emulator_path = os.path.dirname(os.path.abspath(__file__)) + "/../"
sys.path.append(emulator_path)

import jbpf_lcm_api
import jbpf_helpers
import jbpf_test_def
import jbpf_hooks
import emulator_utils

def io_channel_check_output(bufs, num_bufs, ctx):
    for i in range(num_bufs):
        # Get each buffer pointer from the void ** array
        buf_capsule = bufs[i]

        # Get the pointer from the capsule
        buf_pointer = emulator_utils.decode_capsule(buf_capsule, b"void*")

        buffer_size = 1
        int_array_type = ctypes.POINTER(ctypes.c_int * buffer_size)

        # Cast the void pointer to the correct type
        buffer_array = ctypes.cast(buf_pointer, int_array_type)
        print(f"data received: {buffer_array.contents[0]}")

codelet_descriptors = emulator_utils.yaml_to_json(
    JBPF_PATH + "/src/emulator/test/test.yaml", placeholders={"JBPF_PATH": JBPF_PATH}
)
stream_id = codelet_descriptors["codelet_descriptors"][0]["out_io_channel"][0][
    "stream_id"
]
## convert the stream_id to the correct type
stream_id_c1 = emulator_utils.convert_string_to_stream_id(stream_id)
codelet_descriptors["codelet_descriptors"][0]["out_io_channel"][0]["stream_id"] = (
    stream_id_c1
)

## load the jbpf_stats_report codelet
stream_id_c2 = emulator_utils.create_random_stream_id()
codelet_descriptors["codelet_descriptors"].append(
    {
        "codelet_name": "jbpf_stats_report",
        "codelet_path": f"{JBPF_PATH}/tools/stats_report/jbpf_stats_report.o",
        "hook_name": "report_stats",
        "priority": 2,
        "out_io_channel": [
            {
                "name": "output_map",
                "stream_id": stream_id_c2,
                "has_serde": False,
            }
        ],
    }
)

codeletset_req_c1 = emulator_utils.create_codeletset_load_req(codelet_descriptors)

if jbpf_helpers.jbpf_codeletset_load(codeletset_req_c1) != 0:
    print("Failed to load codeletset")
    sys.exit(-1)

## call hook
print("calling hook test1")
p = jbpf_test_def.struct_packet()
for i in range(3):
    p.counter_a = (i + 1) * 10
    res = jbpf_hooks.hook_test1(p, 3)
    if res != 0:
        print(f"hook_test1 returned {res}")
        sys.exit(-1)

## handle output bufs
stream_id_c2 = emulator_utils.create_random_stream_id()
print(f"stream_id_c1: {list(stream_id_c1.id)}")
print(f"stream_id_c2: {list(stream_id_c2.id)}")
emulator_utils.jbpf_handle_out_bufs(
    stream_id_c1, io_channel_check_output, num_of_messages=3, timeout=3, debug=True
)

## unload codeletset
res = emulator_utils.jbpf_codeletset_unload(codeletset_req_c1.codeletset_id)

if res != 0:
    print("Failed to unload codeletset")
    sys.exit(-1)
