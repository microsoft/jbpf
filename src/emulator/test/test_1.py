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

stream_id_c1 = jbpf_lcm_api.struct_jbpf_io_stream_id((0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                         0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF))

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

data = {
    'codeletset_id': 'test_codeletset',
    'num_codelet_descriptors': 1,
    'codelet_descriptors': [
        {
            'num_in_io_channel': 0,
            'num_out_io_channel': 1,
            'num_linked_maps': 0,
            'codelet_name': 'simple_output',
            'hook_name': 'test1',
            'codelet_path': f"{JBPF_PATH}/jbpf_tests/test_files/codelets/simple_output/simple_output.o",
            'priority': 1,
            'runtime_threshold': 1000000000, # 1 second
            'out_io_channel': [
                {
                    'name': 'output_map',
                    'stream_id': stream_id_c1,
                    'has_serde': False
                }
            ]
        }
    ]
}

codeletset_req_c1 = emulator_utils.create_codeletset_load_req(data)

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
emulator_utils.jbpf_handle_out_bufs(stream_id_c1, io_channel_check_output, num_of_messages=3, timeout=3)

## unload codeletset
res = emulator_utils.jbpf_codeletset_unload(codeletset_req_c1.codeletset_id)

if res != 0:
    print("Failed to unload codeletset")
    sys.exit(-1)
