# How to integrate *libjbpf* with my project? 


This document discusses how to build your project with *libjbpf* library. 
For full functionality, one also needs to add [hook](./add_new_hook.md), [maps](./maps.md) or [helper functions](./add_helper_function.md).


*libjbpf* is a light-weight library with a limited number of external dependencies. It creates up to three support threads:
- *Maintenance thread* (always created) is a lightweight thread that runs various management tasks, such as stats updates.
- *Agent thread* (optional) implements the [life-cycle management functionality](./life_cycle_management.md), if enabled. 
- *I/O thread* (optional) implements the [data collection and control functionality](./data_io.md), if enabled. 


To integrate *libjbpf* with a new project, one needs to build the project with the *libjbpf* library, and to initialize it correctly. 
The library can be built as static (`libjbpf.a`) or dynamic (`libjbpf.so`).

## Building with Docker
To avoid installing dependencies locally, you can build the library using a Docker container.

### Steps to Build with Docker
1. Build the container image:
```bash
OS=mariner  # or ubuntu22.04, ubuntu20.04
DEST_PATH=the_abs_path_to_output_folder
sudo -E docker build -t jbpf_lib -f deploy/${OS}.Dockerfile .
```

3. Build the library using the container image:
```bash
sudo -E docker run -v $DEST_PATH:/jbpf_out_lib \
     -e OPTION_NAME1={0,1} \
     -e OPTION_NAME2={0,1} \
     jbpf_lib
```

The `$DEST_PATH` should be set to the absolute path of the directory where you want the output to be stored.

### Available Build Options:
When building the library, you can pass various options using the -e OPTION_NAME={0,1} format where 0 is disabled and 1 is enabled. Here are some available options:

* JBPF_STATIC - Build jbpf as a static library (**default: disabled**). With this, we can also set to value 2 which means to build the jbpf in both `libjbpf.so` and `libjbpf.a`.
* USE_NATIVE - Enable/disable `-march=native` compilation flag (**default: enabled**)
* USE_JBPF_PERF_OPT - Performance optimizations that assume threads calling jbpf codelets are pinned to a certain core (**default: enabled**)
* USE_JBPF_PRINTF_HELPER - Disable the use of the helper function jbpf_printf_debug() (**default: enabled**)
* JBPF_THREADS_LARGE - Allow more threads to be registered by jbpf and the IO lib (**default: disabled**)
* JBPF_EXPERIMENTAL_FEATURES - Enable experimental features of jbpf (**default: disabled**)

Additionally, you can change the `CMAKE_BUILD_TYPE` to:
* Release - Default Build Type.
* AddressSanitizer - Build with AddressSanitizer support to check for memory leaks.
* Debug - Build with jbpf debug symbols.

For a up-to-date list of available options, refer to the [CMakelists.txt](../CMakeLists.txt) file.

### Output
The generated library will be stored in the `$DEST_PATH/out` directory. Other useful files will be located in the out directory for inclusion in your build.

## *libjbpf* initialization

The library initialization requires two steps. The first step is to call `jbpf_init()` function with the desired configuration parameters.
Below is the list of [configuration parameters](../src/core/jbpf_config.h): 
- *Memory* (`jbpf_config.mem_config`): The total size of memory (hugepages) allocated for maps, etc. 
- *Life-cycle management model* (`jbpf_config.lcm_ipc_config`): Specify whether to use C API or Unix sockets for life-cycle management (see [example](../examples/first_example_standalone/example_app.cpp)). 
- *I/O model* (`jbpf_config.io_config.io_type`): Specify whether to use an internal thread with call-backs, or an external data collector over IPC (see [example](../examples/first_example_ipc/example_app.cpp)).
- *Library thread config* (`jbpf_config.io_config` and `jbpf_config.thread_config`): 
  Specify Linux thread priority, CPU affinity and scheduling policy (`SCHED_OTHER` or `SCHED_FIFO`) for various *libjbpf* threads. 


### Customize logging

To set the logging level, you can include the following in your source:
```C
#include "jbpf_logging.h"
logging_level jbpf_logger_level;
jbpf_logger_level = JBPF_INFO; // JBPF_DEBUG, JBPF_INFO, JBPF_WARN, JBPF_ERROR, JBPF_CRITICAL
```

You can also provide a customized logger function, such as:
```C
void jbpf_custom_logger(jbpf_logging_level level, const char* s, ...) {
  if (level >= jbpf_logger_level) {
    print_log_level(level);
    va_list ap;
    va_start(ap, s);
    FILE *where = level >= ERROR ? stderr : stdout;
    vfprintf(where, s, ap);
    va_end(ap);
  }
}
jbpf_va_logger_cb jbpf_va_logger_func = jbpf_custom_logger;
```




## Thread registration 

*libjbpf* often runs concurrent operations, including memory management, access to running codelets, etc. 
In order to implement them efficiently, we need to register each thread that invokes a hook, before the invocation takes place. 
This is done with `jbpf_register_thread()` call, typically at the start of the thread. 
If this is not done, a hook invocation will cause a segfault. When a thread finishes, it is recommended to call `jbpf_cleanup_thread()`.
