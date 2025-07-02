# Copyright (c) Microsoft Corporation. All rights reserved.
# This file tests the emulator_utils.create_random_stream_id function

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
