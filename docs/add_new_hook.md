# Add new hook

In this document we explain how to add a new hook.
We will follow [the simple example](../examples/first_example_standalone/example_app.cpp).

To add a hook, we need to add two things: 
- A declaration and a definition of the hook at the top of the file calling the hook
  (the declaration can be in a separate header file, to be reusable).
- Hook call point
```C
// Declaration and definition
DECLARE_JBPF_HOOK(
    example,
    struct jbpf_generic_ctx ctx,
    ctx,
    HOOK_PROTO(Packet* p, int ctx_id),
    HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))
DEFINE_JBPF_HOOK(example)

// Callpoint
hook_example(&p, 1);
```

In the hook [declaration](../src/core/jbpf_hook.h), we first need to specify the hook name (in this case `example`). 
Then we define an arbitrary custom context to be passed by the hook to the codelet. 
We first give its declaration (in this case `struct jbpf_generic_ctx ctx`), 
and then the argument to be passed to the function (in this case just `ctx`).

Generic context `struct jbpf_generic_ctx` (defined [here](../src/common/jbpf_defs.h)) 
provides a simple context with a pointer to an arbitrary contiguous opaque memory region (`data`), a pointer to the end of the region (`data_end`, to help verification), and a custom `ctx_id`. 
To learn how to create a custom complex, see [below](#create-custom-context).

Next in the definition of a hook is `HOOK_PROTO(Packet* p, int ctx_id)`, specifying the hook call point signature. 
This has to match the hook call point in the application, shown above. 
The naming convention for the hook is `hook_<hook name>` where `hook name` is the name used in the hook declaration, which is in this case `example`. 

The final argument is the hook assignment code:
```C
HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);)
```
This is a custom code to populate the context that executes verbatim before the hook is actually called. 
In this case we assign the context id, the beginning and the and of the opaque data memory region. 
If required, we could also populate the remaining `uint64_t meta_data` parameter or, in case of a non-generic hook, any other required additional parameter. 

Finally, the hook definition, `DEFINE_JBPF_HOOK(example)`, assigns the various information about the hook in corresponding ELF sections so that we can later parse them and identify the location of the functions.


## How to create a custom context

For more involved use cases, one can define and use a different, custom context structure. 
Custom contexts have some important limitations. 
Each context has to contain a single set of `data` and `data_end` pointers, and a `meta_data` field. The verifier does not allow us to expose multiple memory regions as it cannot verify more than one. 
This means that one can supply only a single contiguous memory region to the codelet. 
A single region is sufficient to provide access to a C struct, for example, but doesn't allow dereferencing (e.g. access to link lists or similar data structures) because the verifier cannot verify these operations.
More complicated data structures can be supported with [helper functions](./add_helper_function.md).

However, one can add an arbitrary number of static fields to a custom context. 
A typical use case for this is when an application needs to report a common context to all hooks. 
For example, in case of a user-mode networking application, it may want to always report an index of the NIC in question. 
These parameters can be added as an additional variables to a custom context. 

Another example is the custom context `struct jbpf_stats_ctx` for measuring hook runtimes (defined [here](../src/common/jbpf_defs.h)).
This custom context includes the measurement period in the context. 
To see how to define a hook with this context, see [here](../src/core/jbpf_agent_hooks.h).

Once you create a new custom context, it is important to also extend the verifier to support it.
An example of that is given [here](../jbpf_tests/verifier/jbpf_verifier_extension_test.cpp) for the following context structure:
```C
struct my_new_jbpf_ctx {
    uint64_t data;
    uint64_t data_end;
    uint64_t meta_data;
    uint32_t static_field1;
    uint16_t static_field2;
    uint8_t static_field3;
}; 
```
The whole process of extending a verifier is described in more details [here](./verifier.md).




