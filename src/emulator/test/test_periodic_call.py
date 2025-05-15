import os, sys, ctypes

JBPF_PATH = os.getenv("JBPF_PATH")
sys.path.append(JBPF_PATH + "/out/lib/")

## import the one directory up
emulator_path = os.path.dirname(os.path.abspath(__file__)) + "/../"
sys.path.append(emulator_path)

import jbpf_helpers
import emulator_utils


def io_channel_check_output(bufs, num_bufs, ctx):
    print(f"num_bufs: {num_bufs}")
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
        assert buffer_array.contents[0] == 123


codelet_descriptor = emulator_utils.yaml_to_json(
    JBPF_PATH + "/src/emulator/test/test_periodic_call.yaml",
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

## handle output bufs
print(f"stream_id_c1: {list(stream_id_c1.id)}")
count = emulator_utils.jbpf_handle_out_bufs(
    stream_id_c1,
    io_channel_check_output,
    num_of_messages=3,
    timeout=5 * emulator_utils.SEC_TO_NS,
    debug=True,
)
print(count)

## unload codeletset
res = emulator_utils.jbpf_codeletset_unload(codeletset_req_c1.codeletset_id)

if res != 0:
    print("Failed to unload codeletset")
    sys.exit(-1)
