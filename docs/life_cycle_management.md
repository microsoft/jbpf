# Codelet life-cycle management

Since the codelets get detailed access to and potentially control of the host application, it is very important to manage them safely. 
To reduce the attack vector, we make two design assumptions:
- Only codelet stored on a local secure store can be loaded. 
- The loading and unloading process is triggered locally, using C API. 

It is a responsibility of the application developers to:
- Provide a local, secure codelet storage (see [here](./security.md) for a discussion on how to do that)
- Make sure all codelets in the storage are [verified](./verifier.md)
- Provide external access to the life-cycle management (load and unload) API. 

This is a deliberate design decision, given that different projects may have different requirements in this space. 

However, we also give a sample life-cycle management API using sockets and a local storage, to get you started. 
In the following, we describe different options.



## C API

The preferred interface for life-cycle management is the C API. 
It provides the API calls to load and unload codelets. 
These API calls are defined [here](../src/core/jbpf.h).
They are typically called from the application to which jbpf is linked to. 

For an example on how this API is used, check [here](../jbpf_tests/e2e_examples/jbpf_e2e_standalone_test.c). 




## Unix socket API

The local, Unix socket API is enabled by setting the following option:
`jbpf_config.lcm_ipc_config.has_lcm_ipc_thread = true;`
(see [here](../examples/first_example_standalone/example_app.cpp) for an example).
This provides a convenient way to implement life-cycle management from a different process. 

The codelet loading is done using `lcm_cli` tool, available [here](../tools/lcm_cli). 
For a full example, see [here](../examples/first_example_standalone/README.md).



## Integration example

We built a [simple example](../examples/reverse_proxy) to illustrate how the life-cycle management APIs can be used to integrate in a larger project. 
The example shows a reverse proxy that connects to the jbpf's local Unix socket API, and exposes a REST API on a network that translates the requests back and forth. 
Follow the [instructions](../examples/reverse_proxy/README.md) to try it out.

Please note that this is an example for illustrational purposes and not intended for production use. 


