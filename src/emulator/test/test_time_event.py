# Copyright (c) Microsoft Corporation. All rights reserved.
# This test emulates time events, loads a codeletset, invokes a hook, and verifies the output buffers.
# The codelet sends time events, which are validated using the io_channel_check_output function.
# The codeletset is unloaded after the test completes.
import os, sys, ctypes

JBPF_PATH = os.getenv("JBPF_PATH")
sys.path.append(JBPF_PATH + "/out/lib/")

## import the one directory up
emulator_path = os.path.dirname(os.path.abspath(__file__)) + "/../"
sys.path.append(emulator_path)

import jbpf_helpers
import emulator_utils
import jbpf_test_def
import jbpf_hooks

output_data = []

def io_channel_check_output(bufs, num_bufs, ctx):
    print(f"io_channel_check_output called with {num_bufs} bufs", flush=True)
    for i in range(num_bufs):
        # Get each buffer pointer from the void ** array
        buf_capsule = bufs[i]

        # Get the pointer from the capsule
        buf_pointer = emulator_utils.decode_capsule(buf_capsule, b"void*")

        buffer_size = 1
        int_array_type = ctypes.POINTER(ctypes.c_int64 * buffer_size)

        # Cast the void pointer to the correct type
        buffer_array = ctypes.cast(buf_pointer, int_array_type)
        print(f"data received: {buffer_array.contents[0]}")
        output_data.append(buffer_array.contents[0])


codelet_descriptor = emulator_utils.yaml_to_json(
    JBPF_PATH + "/src/emulator/test/test_time_event.yaml",
    placeholders={"JBPF_PATH": JBPF_PATH},
)
stream_id = codelet_descriptor["codelet_descriptor"][0]["out_io_channel"][0][
    "stream_id"
]
## convert the stream_id to the correct type
stream_id_c1 = emulator_utils.convert_string_to_stream_id(stream_id)
codelet_descriptor["codelet_descriptor"][0]["out_io_channel"][0]["stream_id"] = (
    stream_id_c1
)

codeletset_req_c1 = emulator_utils.create_codeletset_load_req(codelet_descriptor)

if jbpf_helpers.jbpf_codeletset_load(codeletset_req_c1) != 0:
    print("Failed to load codeletset")
    sys.exit(-1)

print("Emulated time events", flush=True)
TOTAL_TIME_EVENTS = 10
# Push 100 time events of 1s difference
for i in range(TOTAL_TIME_EVENTS):
    jbpf_helpers.jbpf_add_time_event(i)

print("calling hook test1", flush=True)
p = jbpf_test_def.struct_packet()
for i in range(TOTAL_TIME_EVENTS):
    res = jbpf_hooks.hook_test1(p, 3)
    if res != 0:
        print(f"hook_test1 returned {res}", flush=True)
        sys.exit(-1)    

## handle output bufs
print(f"stream_id_c1: {list(stream_id_c1.id)}", flush=True)
count = emulator_utils.jbpf_handle_out_bufs(
    stream_id_c1,
    io_channel_check_output,
    num_of_messages=TOTAL_TIME_EVENTS,
    timeout=3 * emulator_utils.SEC_TO_NS,
    debug=True,
)
print(f"Received {count} messages.", flush=True)
assert count == TOTAL_TIME_EVENTS

print("Output data:", output_data, flush=True)
assert(len(output_data) == TOTAL_TIME_EVENTS)
for i in range(TOTAL_TIME_EVENTS):
    assert(output_data[i] == i)

## unload codeletset
res = emulator_utils.jbpf_codeletset_unload(codeletset_req_c1.codeletset_id)

if res != 0:
    print("Failed to unload codeletset")
    sys.exit(-1)
