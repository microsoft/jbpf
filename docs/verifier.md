# Verification

In order to guarantee safety, each eBPF bytecode has to be statically verified before execution. 
We use [Prevail eBPF verifier](https://vbpf.github.io/) for this purpose because it is an extensible verifier that can be used in other domains beyond the Linux kernel.

However, we need to extend the verifier to support specific *jbpf* features. 
For example, a context is passed every time a hook is called, and this context points to a fixed-size memory buffer. 
The verifier needs to know about this in order to verify memory access to that buffer from within the codelet. 
Also, the verifier needs to verify calls to helper functions provided by *jbpf*, to make sure that the way the parameters are passed matches the function signature. 

For this reason, we provide the `libjbpf_verifier`, which acts as an extensible wrapper library on top of the prevail verifier.
This library provides an API call [`jbpf_verify()`](./src/verifier/jbpf_verifier.cpp) for verification (see [here](../jbpf_tests/verifier/jbpf_verifier_extension_test.cpp) for an example).
We also provide a simple command-line (CLI) tool based on the this library. 
To see how it works, build the first example, and in the same directory run:
```bash
$ ../../out/bin/jbpf_verifier_cli example_codelet.o
1,0.153241
Program terminates within 158 instructions
$ echo $?
0
```
Number `1` printed on the screen and shell exit code `0` signify that the codelet is successfully verified. 




## Custom verifier extensions


Application developers can introduce new elements that require verifier extensions. 
This is done through the [verifier API](../src/verifier/jbpf_platform.cpp), which allows to extend the verifier's set of objects:
- *Helper function*: see [here](./add_helper_function.md)
- *Program type and context*: see [here](./add_new_hook.md#how-to-create-a-custom-context).
- *Map*: see [here](../jbpf_tests/verifier/jbpf_verifier_extension_test.cpp).



