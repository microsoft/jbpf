# JBPF Emulator
JBPF Emulator provides a testing framework to test the codelets using the Python script. Here is how it works:

- The emulator is to be built once per project with the custom hooks and helper functions.
- Then the emulator is executed to run the test scripts in Python for the project.

## Build dummy emulator
The dummy emulator (without custom hooks) can be built together with the JBPF library. To build the emulator:

```bash
cd $JBPF_PATH
mkdir -p build
cd build
cmake ../
make -j jbpf_emulator
```

When JBPF is build statically with `-DJBPF_STATIC=1`, the emulator will be built with the `libjbpf.a` library. Otherwise, the emulator will be built with the `libjbpf.so` library.

After this is built:
- the test emulator (with no custom hooks) will be found at `$JBPF_PATH/out/bin/jbpf_emulator`
- the emulator files required to build your custom emulator will be found at `$JBPF_PATH/out/emulator`

## Build custom emulator
With the files at `$JBPF_PATH/out/emulator`, you can build the custom emulator via the following steps:

### Define custom hooks
You can include your hook definitions in `$JBPF_PATH/out/emulator/test/hooks.h`. For example:

```cpp
#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_defs.h"
#include "../hook_wrapper.h"
#include "Python.h"
#include "nr5g_mac_phy_api.h"

// Include the custom hooks below
#include "jbpf_gnb_flexran_l1_hooks.h"

// Define wrapper, for format, see: https://docs.python.org/3/c-api/arg.html
DEFINE_HOOK_AND_WRAPPER(fapi_gnb_dl_csirs_pdu, tCsiRsPduStruct, "y#iiiiii", int, carrier_idx, cell_id, sfn, slot, nSym)

// Add to the hook definition list below
static PyMethodDef jbpfHookMethods[] = {
    {"fapi_gnb_dl_csirs_pdu", hook_fapi_gnb_dl_csirs_pdu_wrapper, METH_VARARGS, "Hook to process a single Downlink CSIRS PDU."},
    {NULL, NULL, 0, NULL}
};
```

### Convert C structs to Python
In order to define structs in Python, you may need to convert C header files to Python. For example:

```bash
## convert jbpf_lcm_api.h to jbpf_lcm_api.py
ctypesgen -a ../lcm/jbpf_lcm_api.h --no-macro-warnings -I ../io -I ../mem_mgmt -I ../common -o $OUTPUT_DIR/lib/jbpf_lcm_api.py

## patch jbpf_lcm_api.py to add _pack_=1
python3 patch.py $OUTPUT_DIR/lib/jbpf_lcm_api.py
```

### Define custom helper functions
You can add custom helper functions such as xran decoder to the `$JBPF_PATH/out/emulator/test/helper_functions.hpp`

### Build the custom emulator
You would need to define the env variable `JBPF_PATH` and `JBPF_OUT_DIR` to point to the JBPF library path and the output directory respectively. For example:

```bash
export JBPF_PATH=/path/to/jbpf
export JBPF_OUT_DIR=/path/to/jbpf/out
```

If the Cmake option `JBPF_STATIC` is set to `1`, the emulator will link to `$JBPF_OUT_DIR/out/lib/libjbpf.a`, otherwise, it will require `$JBPF_OUT_DIR/out/lib/libjbpf.so`

The following steps are required to build the custom emulator:

```bash
cd $JBPF_PATH/out/emulator
mkdir -p build
cd build
## -DJBPF_STATIC=1 or -DJBPF_STATIC=0
cmake ../
make -j$(nproc)
```

On success, the `jbpf_emulator` will be found at `$JBPF_PATH/out/bin`.

## Run tests
Once emulator is built, you can run it to see the available hooks and helper functions:

```bash
$JBPF_PATH/out/bin/jbpf_emulator
```

To run a test, follow the syntax:

```bash
$JBPF_PATH/out/bin/jbpf_emulator PATH_TO_PYTHON_SCRIPT NAME_OF_PYTHON_SCRIPT
```

For example:

```bash
$JBPF_PATH/out/bin/jbpf_emulator $JBPF_PATH/out/emulator/test test_1
```

On success, the exit code will be zero.
