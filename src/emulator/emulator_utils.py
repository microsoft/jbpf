# Copyright (c) Microsoft Corporation. All rights reserved.
import ctypes
import sys
import jbpf_lcm_api
import jbpf_helpers
import datetime
import random

# Load the Python API
python_api = ctypes.pythonapi

# Default values
DEFAULT_RUNTIME_THRESHOLD = 1000000000
DEFAULT_PRIORITY = 1

# Define the function to get the pointer from a PyCapsule
def decode_capsule(capsule, name):
    python_api.PyCapsule_GetPointer.argtypes = [ctypes.py_object, ctypes.c_char_p]
    python_api.PyCapsule_GetPointer.restype = ctypes.c_void_p
    pointer = python_api.PyCapsule_GetPointer(capsule, name)

    if pointer == 0:
        raise ValueError("Failed to get pointer from capsule")

    return pointer

def create_codeletset_load_req(data):
    global DEFAULT_RUNTIME_THRESHOLD, DEFAULT_PRIORITY
    def read_key(data, key, default=None):
        return data.get(key, default)

    req = jbpf_lcm_api.jbpf_codeletset_load_req_s()

    # Populate codeletset_id
    req.codeletset_id.name = bytes(data['codeletset_id'], 'utf-8')

    # Set number of descriptors
    req.num_codelet_descriptors = read_key(data, 'num_codelet_descriptors', 0)

    # Populate each codelet descriptor
    for i, descriptor_data in enumerate(data['codelet_descriptors']):
        descriptor = req.codelet_descriptor[i]

        out_io_channel = descriptor_data.get('out_io_channel', [])
        in_io_channel = descriptor_data.get('in_io_channel', [])
        linked_maps = descriptor_data.get('linked_maps', [])

        descriptor.num_in_io_channel = read_key(descriptor_data, 'num_in_io_channel', len(in_io_channel))
        descriptor.num_out_io_channel = read_key(descriptor_data, 'num_out_io_channel', len(out_io_channel))
        descriptor.num_linked_maps = read_key(descriptor_data, 'num_linked_maps', len(linked_maps))
        descriptor.codelet_name = bytes(descriptor_data['codelet_name'], 'utf-8')
        descriptor.hook_name = bytes(descriptor_data['hook_name'], 'utf-8')
        descriptor.codelet_path = bytes(descriptor_data['codelet_path'], 'utf-8')
        descriptor.priority = read_key(descriptor_data, 'priority', DEFAULT_PRIORITY)
        descriptor.runtime_threshold = read_key(descriptor_data, 'runtime_threshold', DEFAULT_RUNTIME_THRESHOLD)

        # Populate out_io_channel for each descriptor
        for j, channel_data in enumerate(out_io_channel):
            io_channel = descriptor.out_io_channel[j]
            io_channel.name = bytes(channel_data['name'], 'utf-8')
            io_channel.stream_id = channel_data['stream_id']
            io_channel.has_serde = read_key(channel_data, 'has_serde', False)

            # Populate serde if it exists
            if 'serde' in channel_data:
                io_channel.serde.file_path = bytes(channel_data['serde']['file_path'], 'utf-8')

        # Populate in_io_channel for each descriptor
        for j, channel_data in enumerate(in_io_channel):
            io_channel = descriptor.in_io_channel[j]
            io_channel.name = bytes(channel_data['name'], 'utf-8')
            io_channel.stream_id = channel_data['stream_id']
            io_channel.has_serde = read_key(channel_data, 'has_serde', False)

            # Populate serde if it exists
            if 'serde' in channel_data:
                io_channel.serde.file_path = bytes(channel_data['serde']['file_path'], 'utf-8')

        # Populate linked_maps for each descriptor
        for j, map_data in enumerate(linked_maps):
            linked_map = descriptor.linked_maps[j]
            linked_map.map_name = bytes(map_data['map_name'], 'utf-8')
            linked_map.linked_codelet_name = bytes(map_data['linked_codelet_name'], 'utf-8')
            linked_map.linked_map_name = bytes(map_data['linked_map_name'], 'utf-8')

    return req

def filter_stream_id(the_stream_id, callback):
    def wrapper_func(stream_id, bufs, num_bufs, ctx):        
        # Decode the stream_id
        stream_id_pointer = decode_capsule(stream_id, b"jbpf_io_stream_id_t")
        stream_id_struct = ctypes.cast(stream_id_pointer, ctypes.POINTER(jbpf_lcm_api.jbpf_io_stream_id_t))   
        decoded_stream_id = stream_id_struct.contents.id
        # Check if the decoded stream_id matches the target
        if list(the_stream_id.id) == list(decoded_stream_id):
            return callback(bufs, num_bufs, ctx)
        else:
            return -1
    return wrapper_func

def jbpf_codeletset_unload(codeletset_id):
    req = jbpf_lcm_api.struct_jbpf_codeletset_unload_req()
    req.codeletset_id = codeletset_id
    return jbpf_helpers.jbpf_codeletset_unload(req)

def jbpf_handle_out_bufs(stream_id, callback, num_of_messages=1, timeout=1):    
    return jbpf_helpers.jbpf_handle_out_bufs(filter_stream_id(stream_id, callback), num_of_messages, timeout)

def jbpf_send_input_msg(stream_id, msg):
    return jbpf_helpers.jbpf_send_input_msg(bytes(stream_id), msg, ctypes.sizeof(msg))

def create_random_stream_id():
    stream_id = jbpf_lcm_api.struct_jbpf_io_stream_id()
    sz = len(stream_id.id)
    for i in range(sz):
        stream_id.id[i] = random.randint(0, 255)
    return stream_id
