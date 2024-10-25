# Maps

A codelet has access to its stack and a limited number of other memory locations (such as its context). 
Maps are a class of memory locations that codelets can access. 
They can be used in various ways. 
A codelet uses maps to send data to the output API or receive it from the input API. 
It can also use maps to store data between invocations, or to share data with another codelet. 


## Types of maps

The *jbpf* library supports different kinds of maps (for a full list, see [here](../src/common/jbpf_defs.h)):
- *Array*: A simple array of fixed-size elements (see [example](../examples/first_example_standalone/example_codelet.c)).
- *Hashmap*: A simple key/value store (see [example](../jbpf_tests/test_files/codelets/codelet-hashmap/codelet-hashmap.c)).
- *Input and output API*: Maps to communicate with ring buffers and control API (see [example](../examples/first_example_standalone/example_codelet.c)). 
- *Per CPU maps*: Thread-safe versions of *array* and *hashmap* maps that have a copy per CPU (see [example](../jbpf_tests/test_files/codelets/codelet-per-thread/codelet-per-thread.c))



## Shared maps

*jbpf* allows the sharing of maps between loaded programs for the exchange of data.
We will illustrate this on the the following [test application](../jbpf_tests/tools/lcm_cli/jbpf_lcm_cli_loader_test_with_linked_map.cpp).
Note that this test only tests loading and unloading of codelets with shared maps, and doesn't actually call the codelets.


In [both](../jbpf_tests/test_files/codelets/max_input_shared/max_input_shared.c) [codelets](../jbpf_tests/test_files/codelets/max_output_shared/max_output_shared.c), we define the shared map as any other map: 
```C
struct jbpf_load_map_def SEC("maps") shared_map0 = {
    .type = JBPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = 1,
};
```
and use the same API calls as for any other map, such as:
```C
jbpf_map_update_elem(&shared_map0, &index, &api.command, 0)
```

We link the maps when we load them, in the [codelet load request](../jbpf_tests/tools/lcm_cli/codeletset_req_with_linked_map.yaml).
Maps can only be shared among codelets in the same codelet set, so we create one codelet set with ID `max_linked_map_codeletset`, 
and in the list of descriptors we provide a descriptor for each codelet. 

For the second codelet `max_output_shared`, we add a section:
```C
"linked_maps": [
  {
    "map_name": "shared_map_output0",
    "linked_codelet_name": "max_input_shared",
    "linked_map_name": "shared_map0"
  },
  ...
]
```
which lists all the maps that are to be linked with maps created within other codelets. 
In this case, map `shared_map_output0` in codelet [`max_output_shared`](../jbpf_tests/test_files/codelets/max_output_shared/max_output_shared.c) will be linked to map `shared_map0` in codelet [`max_input_shared`](../jbpf_tests/test_files/codelets/max_input_shared/max_input_shared.c).
For this to succeed, the map definitions need to match, otherwise the loader will report an error. 

Note that the same example would work with two different, unrelated maps if the `linked_maps` section was omitted from the load request.


