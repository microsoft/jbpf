# Basic example of standalone *jbpf* operation

This example showcases a basic *jbpf* usage scenario. It provides a dummy C++ application (`example_app`), that initializes
*jbpf* in standalone mode, and an example codelet (`example_codelet.o`). 
The example demonstrates the following:
1. How to declare and call hooks.
2. How to register handler functions for capturing output data from codelets in standalone mode.
3. How to load and unload codeletsets using the LCM CLI tool (via a Unix socket API).
4. How to send data back to running codelets.

For more details of the exact behavior of the application and the codelet, check [here](../../docs/understand_first_codelet.md). 
You can also check the inline comments in [example_app.cpp](./example_app.cpp)
and [example_codelet.c](./example_codelet.c)


## Usage

This example expects *jbpf* to be built.

To build the example from scratch, we run the following commands:
```sh
$ source ../../setup_jbpf_env.sh
$ make
```

This should produce the binaries `example_app` and `example_codelet.o`.

To bring up the application, we run the following commands:
```sh
$ source ../../setup_jbpf_env.sh
$ ./run_app.sh
```

If successful, we shoud see the following line printed:
```
[JBPF_INFO]: Started LCM IPC server
```

To load the codeletset, we run the following commands on a second terminal window:
```sh
$ source ../../setup_jbpf_env.sh
$ ./load.sh
```

If the codeletset was loaded successfully, we should see the following output in the `example_app` window:
```
[JBPF_INFO]: VM created and loaded successfully: example_codelet
```

After that, the agent should start printing periodical messages (once per second):
```
{
    "seq_no": "2",
    "value": "-2",
    "name": "instance 2"
}

[JBPF_DEBUG]:  Called 2 times so far and received response for seq_no 2
```

To unload the codeletset, we run the command:
```sh
$ ./unload.sh
```

The `example_app` should stop printing the periodical messages and should give the following output:
```
[JBPF_INFO]: VM with vmfd 0 (i = 0) destroyed successfully
```