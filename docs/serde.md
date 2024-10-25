# Serialization of IO messages

**NOTE: This is an experimental feature. It is not meant to be used in production environments. To enable it, you must explicitly build jbpf with the flag `-DJBPF_EXPERIMENTAL_FEATURES=on`.**

*jbpf* provides the capability to serialize the messages sent via the IO channels (e.g., to store to disk or to transmit over the network).
The library is agnostic to the serialization method used to pack the data. Instead, it allows the developers of codelets to define their own 
serialization/deserialization functions for their codelets, in the form of callbacks bundled in a shared `.so` library. 


## Example

A concrete example of this functionality can be found in the [first_example_standalone_json_udp](../examples/experimental/first_example_standalone_json_udp).
This example replicates the functionality of the [first example](./understand_first_codelet.md), with the following differences:

* It introduces a callback function that serializes the output messages in JSON format and sends them out to localhost from a UDP socket.
* It provides a UDP receiver, that collects the serialized messages, parses them and prints the string representation on screen. 

The instructions of how to run the example can be found [here](../examples/experimental/first_example_standalone_json_udp/README.md).

### serde library

The shared library that provides the serialization/deserialization capabilities for a codelet, must implement and expose the following C functions:
```C
int jbpf_io_serialize(void* input_msg_buf, size_t input_msg_buf_size, char* serialized_data_buf, size_t serialized_data_buf_size);

int jbpf_io_deserialize(void* input_msg_buf, size_t input_msg_buf_size, char* serialized_data_buf, size_t serialized_data_buf_size);
```

Both functions are optional, meaning that a developer can choose to only provide a function to serialize or deserialize the data.

This functionality is provided in the example by the file [serde_io_lib_example.cpp](../examples/experimental/first_example_standalone_json_udp/serde_io_lib_example.cpp)
only for the serialization part.

The serde library of a codelet can be loaded as part of the codelet load request, by adding a `serde` section in the `.yaml` request file.
In the example, we modify the file [codeletset_load_request.yaml](../examples/experimental/first_example_standalone_json_udp/codeletset_load_request.yaml), to add the following:

```yaml
serde:
    file_path: ./libserde_io_lib_example.so
```

### Serialization API

The serialization and deserialization of messages can be done using the following API:

```C
int jbpf_io_channel_pack_msg(struct jbpf_io_ctx* io_ctx, jbpf_channel_buf_ptr data, void* buf, size_t buf_len);

jbpf_channel_buf_ptr jbpf_io_channel_unpack_msg(struct jbpf_io_ctx* io_ctx, void* in_data, size_t in_data_size, struct jbpf_io_stream_id* stream_id);
```

The serialized message that is stored in `buf`, when calling `jbpf_pack_msg()` has the following structure:

| stream-id (16 bytes) | serialized_payload |

In the above structure, `serialized_payload` is the output produced by `jbpf_io_serialize()` from the serde library that was loaded along with the codelet.

This serialized structure can either be unpacked using the `jbpf_unpack_msg()` call, when using the jbpf library, or it can be unpacked manually 
when accessed from an external process, like in the example [json_udp_receiver.cpp](../examples/experimental/first_example_standalone_json_udp/json_udp_receiver.cpp).