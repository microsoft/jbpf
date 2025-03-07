# Understand your first codelet


Most developers start by writing a codelet for an application that already has *jbpf* integrated with it. 
Here, we explain in detail a [simple example](../examples/first_example_standalone/README.md) consisting of a dummy application, and a codelet. 



## Application description

For this example we start with an [application](../examples/first_example_standalone/example_app.cpp) that runs an infinite loop and calls the `example` hook every second. 
In each hook call, it passes a sequence count and then increases it. 

To keep things simple, the same application also performs the function of [data collection and control](./jbpf_oss_architecture.png). 
It registers a callback `io_channel_print_output` that gets called every time the codelet sends output data, which prints out the received data in a JSON format. 
The callback also demonstrates how to use the input API by sending back the same information to the codelet. 
To see how the same example can be implemented with separate application and data collection binaries, see [here](../examples/first_example_ipc/).

The [codelet](../examples/first_example_standalone/example_codelet.c) receives the data passed in the hook call and sends it back over the output API. 
It also counts and reports how many times it has been called, and uses a map to store the counter between invocations. 

Here is the sample output of the application:
```
[JBPF_DEBUG]:  Called 2 times so far and received response for seq_no 5

{
    "seq_no": "6",
    "value": "-6",
    "name": "instance 6"
}
```


Next, we discuss various components of the example.


## Codelet load request
The load request describes how the codelet should be loaded and contains all required parameters for the load operation. 
Here we consider a YAML example that can be loaded through the [UNIX life-cycle management socket](../life_cycle_management.md) (the original code [here](../examples/first_example_standalone/codeletset_load_request.yaml)):
```yaml
codelet_descriptor:
  - codelet_name: example_codelet
    codelet_path: ${JBPF_PATH}/examples/first_example_standalone/example_codelet.o
    hook_name: example
    in_io_channel:
      - name: inmap
        stream_id: "11111111111111111111111111111111"
    out_io_channel:
      - name: outmap
        stream_id: 00112233445566778899AABBCCDDEEFF
    priority: 1
codeletset_id: example_codeletset
```
Note that the C API uses a [C struct with same content](../src/lcm/jbpf_lcm_api.h).


The first line in the example defines a unique ID for a codeletset. 
A *codeletset* is a group of codelets that have the same lifecycle and/or share resources (e.g. maps).
A *codelet descriptor* array describes each codelet in the set.
In this simple case we only have one codelet in the set 
(see **TBD** for an example with more codelets in a set and shared maps).

The name of the codelet is `example_codelet`. 
It further specifies which eBPF object file should be loaded as a part of this codelet (`codelet_path`)
and which hook it should be attached to (`hook_name`). 
The choice of hooks depends on what is defined in the application that is supposed to load the hook. 
You can check the [source code](../examples/first_example_standalone/example_app.cpp) of our sample application to see how hook `example` is defined. 
We give a tutorial on how to add new hooks [here](./add_new_hook.md).

The descriptor also defines different maps used to share data with the codelet. 
In this case, we define `outmap` as an output API channel, and an `inmap` as an input API channel. 
Each channel has to be given a unique name and a unique 16 bytes hex stream ID.

Finally, we specify the `priority`, which defines in which order should codelets be called if there are multiple assigned to the same hook.
If this field is omitted, all codelets have the same priority and will be run in the order of loading. 




## Codelet C code

We next describe important parts of the codelet [code](../examples/first_example_standalone/example_codelet.c). 


### Maps

Maps are used for a codelet to communicate data with the environment. 
In this example, we use three different maps, illustrating different map use cases. 

The first one is the output map `outmap`, also defined in the codelet descriptor.
It implements the output API, and is used to send data out.
The second one is the input map `inmap`, also defined in the codelet descriptor.
It implements the input API over which the framework can control the behaviour of the codelet externally. 
Both are also described in the codelet descriptor, since they have to be visible externally, to the API. 

The third map stores the counter between consecutive executions. 
The codelet environment is stateless. 
All static memory structures will be deleted before the next invocation. 
In order maintain a persistent state across invocations, we need store it in a map. 
```C
struct jbpf_load_map_def SEC("maps") counter = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
```
This map is an array map with one element, storing an integer 
(the field `key_size` is always of integer size for arrays, but will be different for other map types). 
The map is internal to the codelet, so does not need to be declared in the descriptor. 




### Main code

The main code starts with 
```C
SEC("jbpf_generic")
uint64_t
jbpf_main(void* state) {
```
This is a call signature for the hook. 
The first line defines the type of the program and its context, so that the verifier can check it. 
The context `state` is defined with the hook, as we discuss when describing [how to add a hook](./add_new_hook.md).
If no program type is given, *jbpf* defaults to `jbpf_generic`.


In the body of the code, we first fetch the counter and increment it:
```C
c = jbpf_map_lookup_elem(&counter, &index);
if (!c)
    return 1;

cnt = *(int*)c;
cnt++;
*(uint32_t*)c = cnt;
```
In order to save the counter value between two hook calls, we have to store it in a map. 
For this, we use the single element map array `counter` we have defined earlier. 
We fetch a pointer to a value in the map and increment it.
Before incrementing the value, we need to make sure that the returned pointer `c` is not-NULL, otherwise the codelet will be flagged as unsafe by the verifier.


Next, we send the current counter value to the application:
```C
p = (struct Packet*)ctx->data;
p_end = (struct Packet*)ctx->data_end;
if (p + 1 > p_end) return 1;

echo = *p;
jbpf_ringbuf_output(&outmap, &echo, sizeof(echo));
```
We first access the dynamic memory region from the context and cast it into the desired data structure. 
We need to explicitly verify that the size of that structure is less than or equal to the size of the dynamic memory region, to pass the verification. 
We then send the value received in the context back through the output API using `outmap` we previously defined. 

Finally, the codelet checks the input API for any control messages using `jbpf_control_input_receive` call and prints the value out (note that the print function `jbpf_printf_debug` is only enabled in debug mode). 



## Build process

A typical codelet build process consists of several steps:
- *Source code compilation*: A codelet written in a high-level language is compiled into eBPF bytecode with `clang` compiler. To see an example, check `codelet` section of the [example Makefile](../examples/first_example_standalone/Makefile).
- *Verification*: The eBPF bytecode is statically verified using an eBPF verifier. We use [Prevail verifier](https://vbpf.github.io/) since it is independent of the Linux kernel. To verify this codelet, follow the steps [here](./verifier.md).
- *JIT compilation*: Once verified, the eBPF bytecode is translated into the machine specific instruction set to optimize performance. This process is performed within the `jbpf_codeletset_load` API call. 


