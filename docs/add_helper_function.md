# Add new helper function

Helper functions provide extra functionality to jbpf codelets. 
Standard jbpf helper functions are listed [here](../src/core/jbpf_helper_impl.c), 
and provide access to maps, control input, time, print, etc.  

Developers can add new helper functions. 
For example, these can provide access to more complicated memory structures that cannot be passed within a context (see a discussion [here](./add_new_hook.md)), 
or allow codelet to affect the behaviour of the application in some way. 

In [this example](../jbpf_tests/functional/verifier_extensions/jbpf_external_helper_test.c) we register the following helper function:
```C
jbpf_helper_func_def_t helper_func = {
    "new_helper_func", CUSTOM_HELPER_START_ID, (jbpf_helper_func_t)new_helper_implementation};
```
A new helper function `new_helper_func` is added to a static array `helper_funcs` of helper functions, defined [here](../src/core/jbpf_helper_impl.c), at a unique position defined by `CUSTOM_HELPER_START_ID`.
To simplify the integration with the *ubpf* framework, all helper functions have the same signature:
```C
typedef uint64_t (*jbpf_helper_func_t)(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4);
```
but different arguments can be interpreted within the function in different ways (for example, a `uint64_t` can be interpreted as a pointer).
The function is then registered with the following call:
```C
jbpf_register_helper_function(helper_func)
```

A new helper function also needs to be registered in the verifier.
Here is an [example](../jbpf_tests/functional/verifier_extensions/jbpf_external_helper_test.c) of a helper function:
```C
static int
new_helper_implementation(void* jbpf_ctx, int a, int b, int c)
```
The verifier registration, defined [here](../jbpf_tests/verifier/jbpf_verifier_extension_test.cpp), is:
```C
static const struct EbpfHelperPrototype jbpf_new_helper_proto = {
    .name = "new_helper",
    .return_type = EBPF_RETURN_TYPE_INTEGER, // The function returns an integer
    .argument_type =
        {
            EBPF_ARGUMENT_TYPE_PTR_TO_MAP, // The first argument is a map pointer
            EBPF_ARGUMENT_TYPE_ANYTHING,   // The next three arguments are generic values
            EBPF_ARGUMENT_TYPE_ANYTHING,
            EBPF_ARGUMENT_TYPE_ANYTHING,
        },
};
```
As explained above, *ubpf* integration requires that each function has to have up to five arguments that map to type `uint64_t` and has to return a type that maps to `uint64_t`.
But for the verification, it is important to provide more semantics about the arguments. 
In this case, we specify that the first argument is a pointer.
This allows the verifier to check a valid pointer assignment before the function call. 
Check the full list of arguments in the Prevail verifier [code](../3p/ebpf-verifier/src/ebpf_base.h).

Finally, the function is registered with the verifier with:
```C
jbpf_verifier_register_helper(CUSTOM_HELPER_START_ID, jbpf_new_helper_proto);
```

The whole process of extending a verifier is described in more details [here](./verifier.md).





