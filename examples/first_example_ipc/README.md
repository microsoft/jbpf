# Basic example of standalone *jbpf* operation

This example showcases a basic *jbpf* usage scenario, when using in IPC mode. It provides a C++ application (`example_collect_control`)
 that initializes *jbpf* in IPC primary mode, a dummy C++ application (`example_app`), that initializes
*jbpf* in IPC secondary mode, and an example codelet (`example_codelet.o`). 
The example demonstrates the following:
1. How to declare and call hooks in the *jbpf* secondary process.
2. How to collect data sent by the codelet from the *jbpf* primary process.
3. How to load and unload codeletsets using the LCM CLI tool (via a Unix socket API).
4. How to send data back to running codelets from the *jbpf* primary process.

For more details of the exact behavior of the application and the codelet, you can check the inline comments in [example_collect_control.cpp](./example_collect_control.cpp),
[example_app.cpp](./example_app.cpp) and [example_codelet.c](./example_codelet.c)

## Usage

This example expects *jbpf* to be built.

To build the example from scratch, we run the following commands:
```sh
$ source ../../setup_jbpf_env.sh
$ make
```

This should produce the binaries `example_collect_control`, `example_app` and `example_codelet.o`.

To bring the primary application up, we run the following commands:
```sh
$ source ../../setup_jbpf_env.sh
$ ./run_collect_control.sh
```

If successful, we shoud see the following line printed:
```
[JBPF_INFO]: Allocated size is 1107296256
```

To bring the primary application up, we run the following commands on a second terminal:
```sh
$ source ../../setup_jbpf_env.sh
$ ./run_app.sh
```

If successful, we shoud see the following printed in the log of the secondary:
```
[JBPF_INFO]: Agent thread initialization finished
[JBPF_INFO]: Setting the name of thread 1035986496 to jbpf_lcm_ipc
[JBPF_INFO]: Registered thread id 1
[JBPF_INFO]: Started LCM IPC thread at /var/run/jbpf/jbpf_lcm_ipc
[JBPF_DEBUG]: jbpf_lcm_ipc thread ready
[JBPF_INFO]: Registered thread id 2
[JBPF_INFO]: Started LCM IPC server
```

and on the primary:
```
[JBPF_INFO]: Negotiation was successful
[JBPF_INFO]: Allocation worked for size 1073741824
[JBPF_INFO]: Allocated size is 1073741824
[JBPF_INFO]: Heap was created successfully
```

To load the codeletset, we run the following commands on a third terminal window:
```sh
$ source ../../setup_jbpf_env.sh
$ ./load.sh
```

If the codeletset was loaded successfully, we should see the following output in the `example_app` window:
```
[JBPF_INFO]: VM created and loaded successfully: example_codelet
```

After that, the primary `example_collect_control` should start printing periodical messages (once per second):
```
{
    "seq_no": "2",
    "value": "-2",
    "name": "instance 2"
}
```
Similarly, the secondary `example_app` should also print periocical messages:
```
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