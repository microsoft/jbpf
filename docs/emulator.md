# JBPF Emulator

**JBPF Emulator** is a testing framework that lets you test *codelets* by writing and running Python scripts. It’s especially useful for validating your logic without deploying it into a real runtime environment.

## What Is the Emulator?

Think of the emulator as a standalone binary that acts like the JBPF runtime. You build it once for your project and then use Python scripts to simulate and test JBPF codelets.

There are two kinds of emulator builds:
1. **Dummy Emulator**: Basic emulator with no custom logic, which is built by `make -j jbpf_emulator`.
2. **Custom Emulator**: Extended with your hooks and helper functions for real test scenarios.

## Workflow of Emulator
Jbpf emulator is a C++ app that starts jbpf. When it starts, it does the following:
- Loads the JBPF runtime.
- Registers custom hooks.
- Registers custom helper functions.
- Loads user's python code. The python code is executed and users can load codelet, unload codelet, calls hooks, make assertions in the similar fashion as those in JBPF C tests.
- When the python code is done, the emulator exits after cleaning up the JBPF runtime.

---

## Building the Dummy Emulator

The dummy emulator (in [emulator](../src/emulator/)) includes no custom hooks. It's mainly used to verify that everything builds correctly.

### Steps

```bash
cd $JBPF_PATH  
mkdir -p build  
cd build  
cmake ..  
make -j jbpf_emulator  
```

### Output

- Binary: `$JBPF_PATH/out/bin/jbpf_emulator`
- Files to create custom emulator: `$JBPF_PATH/out/emulator`

> Note: If you're using static linking (`-DJBPF_STATIC=1`), the emulator links to `libjbpf.a`. Otherwise, it links to `libjbpf.so`.

---

## Building a Custom Emulator

The custom emulator allows you to register your own hooks and helper functions to simulate actual JBPF behavior during tests. You can add custom hooks and helper functions to extend the emulator's functionality. The emulator files are located in the `$JBPF_OUT_DIR/emulator` directory, which is created when you build the dummy emulator.

### 1. Prepare Environment

```bash
export JBPF_PATH=/path/to/jbpf  
export JBPF_OUT_DIR=/path/to/jbpf/out  
```

### 2. Define Custom Hooks

Edit the file: `$JBPF_OUT_DIR/emulator/hooks.h`

Example:

```c
#include "jbpf.h"  
#include "jbpf_hook.h"  
#include "jbpf_defs.h"  
#include "Python.h"  
#include "custom_headers.h"  
  
// Custom hooks  
#include "custom_hooks.h"  
  
// Wrapper format: https://docs.python.org/3/c-api/arg.html  
DEFINE_HOOK_AND_WRAPPER(fapi_gnb_dl_csirs_pdu, tCsiRsPduStruct, "y#iiiiii", int, carrier_idx, cell_id, sfn, slot, nSym)  
  
// Hook registration  
static PyMethodDef jbpfHookMethods[] = {  
    {"fapi_gnb_dl_csirs_pdu", hook_fapi_gnb_dl_csirs_pdu_wrapper, METH_VARARGS, "Process a Downlink CSIRS PDU."},  
    {NULL, NULL, 0, NULL}  
};  
```

### 3. Convert C Headers to Python

Use `ctypesgen` to generate Python bindings for your structs:

```bash
ctypesgen -a ../lcm/jbpf_lcm_api.h --no-macro-warnings -I ../io -I ../mem_mgmt -I ../common -o $OUTPUT_DIR/lib/jbpf_lcm_api.py  
```

Optionally patch the output to add `__pack__ = 1`:

```bash
python3 patch.py $OUTPUT_DIR/lib/jbpf_lcm_api.py  
```

### 4. Add Custom Helper Functions

You can define additional helper functions in the [`$JBPF_OUT_DIR/emulator/helper_functions.hpp`](../src/emulator/helper_functions.hpp) file.

### 5. Build the Custom Emulator

```bash
cd $JBPF_OUT_DIR/emulator  
mkdir -p build  
cd build
  
# Use -DJBPF_STATIC=1 if building with static libjbpf  
cmake -DJBPF_STATIC=1 ..  
make -j$(nproc)  
```

### Output

Custom emulator binary: `$JBPF_PATH/out/bin/jbpf_emulator`

---

## Example Tests

There are a few sample test scripts located in [$JBPF_OUT_DIR/emulator/test](../src/emulator/test/).

---

## Running Tests

To inspect available hooks and functions, simply run the emulator without arguments:

```bash
$JBPF_OUT_DIR/bin/jbpf_emulator  
```

To run a test:

```bash
$JBPF_OUT_DIR/bin/jbpf_emulator test_directory python_script_name_without_py_extension
```

Example:

```bash
$JBPF_OUT_DIR/bin/jbpf_emulator $JBPF_OUT_DIR/emulator/test test_1  
```

If successful, the emulator exits with code `0`.

## Emulator APIs

This section documents the high-level API flow when using the JBPF Emulator in Python.

### Python Imports
You would typicall import the following modules in your Python scripts:

```python
import jbpf_helpers
import emulator_utils
import jbpf_hooks
```

#### jbpf_helpers
The `jbpf_helpers` module provides access to the JBPF runtime functions, such as loading codeletsets, sending messages, and handling time events. It includes functions like `jbpf_codeletset_load`, `jbpf_codeletset_unload`, and `jbpf_add_time_event`.

#### emulator_utils
The `emulator_utils` module provides utility functions to interact with the JBPF emulator. It includes functions for sending messages, handling output buffers, and managing codeletsets, see below for details.

#### jbpf_hooks
This module contains the hook functions defined in your custom hooks file. You can call these hooks directly in your Python scripts to trigger the corresponding logic in your codelets.

For example, if you defined a hook named `hook_test1` in your custom hooks, you can call it like this:

```python
import jbpf_hooks
p = jbpf_test_def.struct_packet()  # Assuming you have a struct_packet defined
for _ in range(10):
    jbpf_hooks.hook_test1(p, 1)  # Call the hook with the required parameters
``` 

### Load Codeletset

To load a codeletset into the emulator:

```python
import emulator_utils
import jbpf_helpers

codeletset_req = emulator_utils.create_codeletset_load_req(codelet_descriptor)  
res = jbpf_helpers.jbpf_codeletset_load(codeletset_req)  
if res != 0:  
  print("Failed to load codeletset")  
  sys.exit(-1)
```

The `codelet_descriptor` should define:  
- `codeletset_id`  
- One or more `codelet_descriptor` entries, each specifying:  
  - `codelet_name`, `hook_name`, `codelet_path`  
  - Input/output IO channels with `stream_id`  
  - Optional `priority`, `runtime_threshold`, etc.

---

### Unload Codeletset `jbpf_codeletset_unload(codeletset_id)`

To unload the codeletset from the JBPF emulator runtime:

```python
import emulator_utils

res = emulator_utils.jbpf_codeletset_unload(codeletset_req.codeletset_id)  
if res != 0:  
  print("Failed to unload codeletset")  
  sys.exit(-1)
```

> **Args:**
> - `codeletset_id`: The `jbpf_codeletset_id_t` to unload.

> **Returns:** `int` (0 on success)

### Send Input Message `jbpf_send_input_msg(stream_id, msg)`
Sends a structured message to a stream's input channel.

> **Args:**
> - `stream_id`: Target stream ID (`jbpf_io_stream_id_t`).
> - `msg`: ctypes structure (e.g., `struct__custom_api`).

> **Returns:** `int` (0 on success)

Example:

```python
import emulator_utils

p = jbpf_test_def.struct__custom_api()  
for i in range(5):  
  p.command = i**2  
  emulator_utils.jbpf_send_input_msg(stream_id_c1, p)
```

This simulates a stream of input to your codelet.

### Call Hook

Call the registered hook to trigger the codelet's logic, for example:

```python
import jbpf_hooks

p = jbpf_test_def.struct_packet()
for _ in range(10):
  jbpf_hooks.hook_test1(p, 1)
```

The hooks should be defined in [hooks.h](../src/emulator/hooks.h).

### Handle Output Buffers via Callback `jbpf_handle_out_bufs(stream_id, callback, num_of_messages=1, timeout=DEFAULT_TIMEOUT, debug=False)`

Handles output buffers for a given stream by invoking a callback.

> **Args:**
> - `stream_id`: Stream to listen to.
> - `callback`: Function to process each output message.
> - `num_of_messages`: Number of expected messages.
> - `timeout`: Timeout in nanoseconds.
> - `debug`: Log messages when skipping mismatched stream IDs.

> **Returns:** Number of messages successfully handled.

For example, to handle output from a stream:

```python
import emulator_utils

def io_channel_check_output(bufs, num_bufs, ctx):  
  for i in range(num_bufs):  
    buf_capsule = bufs[i]  
    buf_pointer = emulator_utils.decode_capsule(buf_capsule, b"void*")  
    buffer_array = ctypes.cast(buf_pointer, ctypes.POINTER(ctypes.c_int * 1))  
    print(f"data received: {buffer_array.contents[0]}")  

count = emulator_utils.jbpf_handle_out_bufs(  
  stream_id_c1,    ## stream id for output channel
  io_channel_check_output,  ## callback function
  num_of_messages=5,  ## number of messages to wait for
  timeout=3 * (10**9),   ## max wait time in nanoseconds
  debug=True,  ## enable debug output
)  
assert count == 5
```

### Emulated Time Events

To simulate time advancement in your tests:

```python
import jbpf_helpers

jbpf_helpers.jbpf_add_time_event(time_ns)
```

where `time` is type of integer. You can push the time events and then in your codelet, the `jbpf_time_get_ns` function will return the time you pushed.

### `emulator_utils.decode_capsule(capsule, name)`
Extracts a C pointer from a Python capsule.

> **Args:**
> - `capsule`: Python capsule object.
> - `name`: Capsule name as a bytestring.

> **Returns:** `ctypes.c_void_p` pointer.

Raises a `ValueError` if the capsule is invalid.

### `emulator_utils.create_codeletset_load_req(data)`

Converts a Python `dict` describing the codeletset and its codelets into a `jbpf_codeletset_load_req_s` struct for loading.

> **Args:**
> - `data`: Dictionary with keys like `codeletset_id`, `codelet_descriptor`, etc.

> **Returns:** `jbpf_codeletset_load_req_s` struct

### `emulator_utils.filter_stream_id(the_stream_id, callback, debug)`

Wraps an output callback to **filter by stream ID**. Only invokes the callback if the actual stream matches the expected one.

> **Args:**
> - `the_stream_id`: Stream to match.
> - `callback`: Function `(bufs, num_bufs, ctx)` to invoke.
> - `debug`: If `True`, logs mismatches.

> **Returns:** Wrapped callback function.

### `emulator_utils.create_random_stream_id()`

Generates a random 16-byte stream ID.

> **Returns:** `struct_jbpf_io_stream_id` object with random `.id[]` field.

### `emulator_utils.yaml_to_json(file_path, placeholders=None)`

Parses a YAML file, replaces placeholders, and converts the result into a JSON-compatible Python `dict`.

> **Args:**
> - `file_path`: Path to `.yaml` file.
> - `placeholders`: Optional dictionary for string substitution (`{{PLACEHOLDER}}`).

> **Returns:** Python `dict` (or `None` on error)

### `emulator_utils.convert_string_to_stream_id(stream_id_str)`

Converts a 16-character string or 16-byte object into a `jbpf_io_stream_id_t`.

> **Args:**
> - `stream_id_str`: Must be a string or bytes-like object of length 16.

> **Returns:** `jbpf_io_stream_id_t`

Raises `TypeError` or `ValueError` if format is invalid.

## Summary

| Task | Command/Path |
|------|--------------|
| Build dummy emulator | `make -j jbpf_emulator` |
| Dummy binary | `$JBPF_PATH/out/bin/jbpf_emulator` |
| Custom build files | `$JBPF_PATH/out/emulator/` |
| Add hooks | `hooks.h` |
| Add helpers | `helper_functions.hpp` |
| Build custom emulator | `cmake && make` in `/out/emulator/build` |
| Run test | `jbpf_emulator <dir> <script>` |
| Emulated time event | `jbpf_helpers.jbpf_add_time_event(time)` |
| Load codeletset | `jbpf_helpers.jbpf_codeletset_load(req)` |
| Unload codeletset | `jbpf_helpers.jbpf_codeletset_unload(req.codeletset_id)` |
| Send input message | `emulator_utils.jbpf_send_input_msg(stream_id, message)` |
| Call hook | `jbpf_hooks.hook_name(args)` |
| Handle output buffers | `emulator_utils.jbpf_handle_out_bufs(stream_id, callback, num_of_messages, timeout, debug)` |
| Decode capsule | `emulator_utils.decode_capsule(capsule, name)` |
| Create codeletset load request | `emulator_utils.create_codeletset_load_req(data)` |
| Filter stream ID | `emulator_utils.filter_stream_id(stream_id, callback, debug)` |
| Create random stream ID | `emulator_utils.create_random_stream_id()` |
| YAML to JSON | `emulator_utils.yaml_to_json(file_path, placeholders)` |
| Convert string to stream ID | `emulator_utils.convert_string_to_stream_id(stream_id_str)` |
