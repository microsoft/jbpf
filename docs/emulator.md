# JBPF Emulator

**JBPF Emulator** is a testing framework that lets you test *codelets* by writing and running Python scripts. It’s especially useful for validating your logic without deploying it into a real runtime environment.

This README explains:
- What the emulator does
- How to build it (dummy and custom versions)
- How to add your own hooks and helpers
- How to run tests using the emulator

---

## What Is the Emulator?

Think of the emulator as a standalone binary that acts like the JBPF runtime. You build it once for your project and then use Python scripts to simulate and test JBPF codelets.

There are two kinds of emulator builds:
1. **Dummy Emulator**: Basic emulator with no custom logic (good for testing the build setup).
2. **Custom Emulator**: Extended with your hooks and helper functions for real test scenarios.

---

## Building the Dummy Emulator

The dummy emulator includes no custom hooks. It's mainly used to verify that everything builds correctly.

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

The custom emulator allows you to register your own hooks and helper functions to simulate actual JBPF behavior during tests.

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

---

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


