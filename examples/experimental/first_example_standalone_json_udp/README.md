# Example of JSON serialization

**Note: This example leverages an experimental feature. It is not meant to be used in production environments. 
For the example to work as expected, you must explicitly build jbpf with the flag `-DJBPF_EXPERIMENTAL_FEATURES=on`.** 

This example replicates the [first_example_standalone](../../first_example_standalone), but adds JSON serialization and transmission of messages via UDP.
It loads a codelet along with a serialization library, which allows the codelet output messages to be serialized in JSON format
and sent out via a UDP socket. 
A UDP receiver collects the messages, parses them and prints the JSON message on screen.

## How to build and run

For the example to work, you need to make sure that `$JBPF_PATH` is correctly set.

The example is automatically built through the cmake build system.
It can also be built by running `make` in the root directory. 

On one terminal, bring up the example_app, by running:
```bash
example_app
```

On a second terminal, bring up the UDP receiver:
```bash
json_udp_receiver
```

Finally, load the codelet to the example_app, by running the `load.sh` script.

If all worked well, you should see the received JSON messages printed on the screen of the receiver:
```json
Received message: {
    "seq_no": "12",
    "value": "-12",
    "name": "instance 12"
}
```

To unload the codelet, run the `unload.sh` script.