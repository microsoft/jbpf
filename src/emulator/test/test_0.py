# Copyright (c) Microsoft Corporation. All rights reserved.
# Smoke tests for stream ids and emulator API validation.

import os, sys, time, ctypes

JBPF_PATH = os.getenv("JBPF_PATH")
sys.path.append(JBPF_PATH + "/out/lib/")

import jbpf_lcm_api
import jbpf_helpers
import jbpf_test_def
import jbpf_hooks

emulator_path = os.path.dirname(os.path.abspath(__file__)) + "/../"
sys.path.append(emulator_path)

import emulator_utils

print("Hello from test_0.py")

for _ in range(5):
    id = emulator_utils.create_random_stream_id()
    print(f"Random stream id: {list(id.id)}")

assert jbpf_helpers.__name__ == "jbpf_helpers"

for helper in (
    jbpf_helpers.jbpf_codeletset_load,
    jbpf_helpers.jbpf_codeletset_unload,
):
    for data in (b"", b"\0"):
        try:
            helper(data)
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid codeletset request size was accepted")

for size in (-1, 2):
    try:
        jbpf_helpers.jbpf_send_input_msg(bytes(16), b"\0", size)
    except ValueError:
        pass
    else:
        raise AssertionError("Invalid input message size was accepted")

for num_of_messages, timeout in ((-1, 0), (1, 2**64 - 1)):
    try:
        jbpf_helpers.jbpf_handle_out_bufs(lambda *args: None, num_of_messages, timeout)
    except ValueError:
        pass
    else:
        raise AssertionError("Invalid output wait arguments were accepted")

timeout = 100_000_000
start = time.monotonic()
cpu_start = time.thread_time()
assert jbpf_helpers.jbpf_handle_out_bufs(lambda *args: None, 1, timeout) == 0
elapsed = time.monotonic() - start
cpu_elapsed = time.thread_time() - cpu_start
assert elapsed >= 0.9 * timeout / emulator_utils.SEC_TO_NS
assert cpu_elapsed < elapsed / 2 + 0.01
