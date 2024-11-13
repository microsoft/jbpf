// Copyright (c) Microsoft Corporation. All rights reserved.
#include <queue>
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <semaphore.h>
#include "jbpf.h"
#include "jbpf_utils.h"
#include "jbpf_test_def.h"
#include "jbpf_logging.h"
#include "jbpf_int.h"
#include "jbpf_agent_hooks.h"
#include <sys/queue.h>

#define PY_SSIZE_T_CLEAN
#undef _GNU_SOURCE
#include <Python.h>

using namespace std;

// user input
#include "test/hooks.h"
#include "test/helper_functions.hpp"

/* Compiler magic to make address sanitizer ignore
   memory leaks originating from libpython */
__attribute__((used)) const char *__asan_default_options() {
  return "detect_leaks=1";
}

__attribute__((used)) const char *__lsan_default_options() {
  return "print_suppressions=0";
}

__attribute__((used)) const char *__lsan_default_suppressions() {
  return "leak:libpython";
}

// sample events;
typedef int16_t XRAN_SAMPLE_DATA_TYPE;

// number of resource block size
#define RB_SIZE (12 * 2)

TAILQ_HEAD(sample_tailhead, sample_event) sample_head;

struct sample_event
{
    // pointer to the sample data
    int pt;
    // actual data
    XRAN_SAMPLE_DATA_TYPE* data;
    // length of the data
    int len;
    // repeat count
    int count;
    TAILQ_ENTRY(sample_event) entries;
};

TAILQ_HEAD(tailhead, time_event) head;

// we take count items from the queue and copy it to out_data
// and return the number of items copied (may be less than count)
// NOTE: This is not thread safe *yet* (need to add lock
int
jbpf_get_xran_samples(XRAN_SAMPLE_DATA_TYPE* out_data, int count)
{
    int ans = 0;
    int data_pt = 0;
    while (ans < count) {
        struct sample_event* elem;
        if (TAILQ_EMPTY(&sample_head)) {
            return ans;
        }
        elem = sample_head.tqh_first;
        // we can still take one item from the current array
        if (elem->pt < elem->len) {
            out_data[data_pt++] = elem->data[elem->pt++];
            ans++;
        } else {
            // if we exhaust the count, we need to free current array
            // and move one to the next one
            elem->pt = 0;
            elem->count--;
            if (elem->count == 0) {
                TAILQ_REMOVE(&sample_head, sample_head.tqh_first, entries);
                free(elem->data);
                free(elem);
            }
        }
    }
    return ans;
}

// we enqueue the data of size len and mark it repeat=count times
int
jbpf_write_xran_samples(XRAN_SAMPLE_DATA_TYPE* data, int len, int count)
{
    struct sample_event* elem;
    elem = static_cast<struct sample_event*>(malloc(sizeof(struct sample_event)));
    if (elem) {
        elem->pt = 0;
        elem->count = count;
        elem->len = len;
        elem->data = static_cast<XRAN_SAMPLE_DATA_TYPE*>(malloc(len * sizeof(XRAN_SAMPLE_DATA_TYPE)));
        memcpy(elem->data, data, len * sizeof(XRAN_SAMPLE_DATA_TYPE));
    } else {
        return -1;
    }
    TAILQ_INSERT_TAIL(&sample_head, elem, entries);
    return 0;
}

static PyObject*
helper_jbpf_write_xran_samples(PyObject* self, PyObject* args)
{
    Py_buffer view;
    int count;
    if (!PyArg_ParseTuple(args, "y*i", &view, &count)) {
        PyErr_SetString(PyExc_TypeError, "expecting bytearray/bytes, count");
        return Py_BuildValue("i", 1);
    }
    XRAN_SAMPLE_DATA_TYPE* data = (XRAN_SAMPLE_DATA_TYPE*)view.buf;
    Py_ssize_t len = view.len / sizeof(XRAN_SAMPLE_DATA_TYPE);
    int ret = jbpf_write_xran_samples(data, len, count);
    PyBuffer_Release(&view);
    return Py_BuildValue("i", ret);
}

static PyObject*
helper_jbpf_clear_xran_samples(PyObject* self, PyObject* args)
{
    int ans = 0;
    while (!TAILQ_EMPTY(&sample_head)) {
        struct sample_event* elem;
        elem = sample_head.tqh_first;
        ans += elem->len - elem->pt + elem->len * (elem->count - 1);
        TAILQ_REMOVE(&sample_head, sample_head.tqh_first, entries);
        free(elem->data);
        free(elem);
    }
    return Py_BuildValue("i", ans);
}

static PyObject*
helper_jbpf_get_xran_samples(PyObject* self, PyObject* args)
{
    int count;
    if (!PyArg_ParseTuple(args, "i", &count)) {
        PyErr_SetString(PyExc_TypeError, "expecting count");
        Py_RETURN_NONE;
    }
    int size_in_bytes = count * sizeof(XRAN_SAMPLE_DATA_TYPE);
    XRAN_SAMPLE_DATA_TYPE* out_data;
    out_data = static_cast<XRAN_SAMPLE_DATA_TYPE*>(malloc(size_in_bytes));
    int ret = jbpf_get_xran_samples(out_data, count);
    if (ret == 0) {
        free(out_data);
        Py_RETURN_NONE;
    }
    PyObject* py_out_data = PyByteArray_FromStringAndSize((char*)out_data, ret * sizeof(XRAN_SAMPLE_DATA_TYPE));
    free(out_data);
    return py_out_data;
}

struct time_event
{
    uint64_t tevent;
    TAILQ_ENTRY(time_event) entries;
};

int
add_time_event(uint64_t event)
{
    struct time_event* elem;
    elem = (time_event*)malloc(sizeof(struct time_event));
    if (elem) {
        elem->tevent = event;
    } else {
        return -1;
    }
    TAILQ_INSERT_TAIL(&head, elem, entries);
    return 0;
}

uint64_t
get_time_event()
{
    struct time_event* elem;
    uint64_t tevent;
    if (TAILQ_EMPTY(&head)) {
        return 0;
    }

    elem = head.tqh_first;
    TAILQ_REMOVE(&head, head.tqh_first, entries);
    tevent = elem->tevent;
    free(elem);

    return tevent;
}

static PyObject*
hook_report_stats_wrapper(PyObject* self, PyObject* args)
{
    struct jbpf_perf_hook_list* hook_perfs;
    size_t size;
    uint32_t meas_period;
    if (!PyArg_ParseTuple(args, "y#I", &hook_perfs, &size, &meas_period)) {
        PyErr_SetString(PyExc_TypeError, "wrong argument type");
        return Py_BuildValue("i", 1);
    }
    if (size != sizeof(struct jbpf_perf_hook_list)) {
        PyErr_SetString(PyExc_TypeError, "wrong buffer size");
        return Py_BuildValue("i", 2);
    }
    hook_report_stats(hook_perfs, meas_period);
    return Py_BuildValue("i", 0);
}

static PyObject*
hook_periodic_call_wrapper(PyObject* self, PyObject* args)
{
    uint32_t meas_period;
    if (!PyArg_ParseTuple(args, "I", &meas_period)) {
        PyErr_SetString(PyExc_TypeError, "wrong argument type");
        return Py_BuildValue("i", 1);
    }
    hook_periodic_call(meas_period);
    return Py_BuildValue("i", 0);
}

static PyMethodDef jbpfAgentHookMethods[] = {
    {"hook_report_stats", hook_report_stats_wrapper, METH_VARARGS, "report_stats"},
    {"hook_periodic_call", hook_periodic_call_wrapper, METH_VARARGS, "periodic_call"},
    {NULL, NULL, 0, NULL}};

static PyModuleDef HelperFunctionModules = {
    PyModuleDef_HEAD_INIT, "helper_functions", NULL, -1, HelperFunctions, NULL, NULL, NULL, NULL};

static PyObject*
PyInit_helper_functions(void)
{
    return PyModule_Create(&HelperFunctionModules);
}

typedef struct message_t
{
    jbpf_io_stream_id_t* stream_id;
    void** data;
    int nbuf;
    void* ctx;
} message_t;

static std::queue<message_t*> message_queue;

static void
io_channel_check_output(jbpf_io_stream_id_t* stream_id, void** bufs, int num_bufs, void* ctx)
{
    // Allocate memory for the message
    message_t* message = (message_t*)malloc(sizeof(message_t));
    message->stream_id = stream_id;

    // Allocate memory for the data pointers
    message->data = (void**)malloc(num_bufs * sizeof(void*));
    if (message->data == NULL) {
        free(message); // Avoid memory leak
        return;        // Handle memory allocation failure
    }

    // Copy the pointers from bufs to message->data
    memcpy(message->data, bufs, num_bufs * sizeof(void*));
    message->nbuf = num_bufs;
    message->ctx = ctx;

    // Push the message to the queue
    message_queue.push(message);
}

static PyModuleDef jbpfHookModule = {
    PyModuleDef_HEAD_INIT, "jbpf_hooks", NULL, -1, jbpfHookMethods, NULL, NULL, NULL, NULL};

static PyModuleDef jbpfAgentHookModule = {
    PyModuleDef_HEAD_INIT, "jbpf_agent_hooks", NULL, -1, jbpfAgentHookMethods, NULL, NULL, NULL, NULL};

static PyObject*
PyInit_jbpf_hooks(void)
{
    return PyModule_Create(&jbpfHookModule);
}

static PyObject*
PyInit_jbpf_agent_hooks(void)
{
    return PyModule_Create(&jbpfAgentHookModule);
}

static PyObject*
helper_jbpf_load_codeletset(PyObject* self, PyObject* args)
{
    struct jbpf_codeletset_load_req* codeletset_req_c1;
    Py_buffer pyBuffer;
    jbpf_codeletset_load_error_s err;

    if (!PyArg_ParseTuple(args, "y*", &pyBuffer)) {
        PyErr_SetString(PyExc_TypeError, "Expected a bytes-like object for codeletset request");
        return Py_BuildValue("i", 1); // Return error code
    }

    codeletset_req_c1 = (struct jbpf_codeletset_load_req*)pyBuffer.buf;

    if (!codeletset_req_c1) {
        PyErr_SetString(PyExc_TypeError, "Invalid codeletset request object");
        PyBuffer_Release(&pyBuffer);
        return Py_BuildValue("i", 1); // Return error code
    }

    int res = jbpf_codeletset_load(codeletset_req_c1, &err);
    PyBuffer_Release(&pyBuffer);

    if (res != 0) {
        jbpf_logger(JBPF_ERROR, "Failed to load codeletset: %s\n", err.err_msg);
    }
    return Py_BuildValue("i", res);
}

static PyObject*
helper_jbpf_unload_codeletset(PyObject* self, PyObject* args)
{
    struct jbpf_codeletset_unload_req* codeletset_unload_req_c1;
    jbpf_codeletset_load_error_s err;
    size_t size;
    if (!PyArg_ParseTuple(args, "y#", &codeletset_unload_req_c1, &size)) {
        PyErr_SetString(PyExc_TypeError, "wrong argument type");
        return Py_BuildValue("i", 1);
    }

    // Unload the codeletset
    int res = jbpf_codeletset_unload(codeletset_unload_req_c1, &err);
    if (res != 0) {
        jbpf_logger(JBPF_ERROR, "Failed to unload codeletset: %s\n", err.err_msg);
    }
    return Py_BuildValue("i", res);
}

// C wrapper that calls the Python callback
void
c_io_output_handler_wrapper(PyObject* py_callback, int num_of_messages, int timeout)
{
    if (!py_callback) {
        return;
    }

    // start time
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    while (num_of_messages > 0) {
        if (timeout > 0) {
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            if (now.tv_sec - start.tv_sec > timeout) {
                break;
            }
        }

        // Pop out all the messages from the queue and re-construct stream_id, buf, nbuf, and ctx
        while (!message_queue.empty()) {
            message_t* message = message_queue.front();
            message_queue.pop();

            PyObject* py_stream_id = PyCapsule_New((void*)message->stream_id, "jbpf_io_stream_id_t", NULL);

            // Create a list to hold the buffer capsules
            PyObject* py_bufs = PyList_New(message->nbuf);
            for (int i = 0; i < message->nbuf; ++i) {
                PyObject* buf_capsule = PyCapsule_New(message->data[i], "void*", NULL);
                PyList_SetItem(py_bufs, i, buf_capsule); // PyList_SetItem takes ownership
            }

            PyObject* py_num_bufs = PyLong_FromLong(message->nbuf);
            PyObject* py_ctx = message->ctx ? PyCapsule_New(message->ctx, "void*", NULL) : Py_None;
            if (py_ctx == Py_None) {
                Py_INCREF(Py_None);
            }

            // Call the Python callback
            PyObject* result =
                PyObject_CallFunctionObjArgs(py_callback, py_stream_id, py_bufs, py_num_bufs, py_ctx, NULL);
            if (!result) {
                PyErr_Print();
                jbpf_logger(JBPF_ERROR, "Error: Python callback failed\n");
            }

            // Clean up Python objects
            Py_XDECREF(result);
            Py_XDECREF(py_stream_id);
            Py_XDECREF(py_bufs);
            Py_XDECREF(py_num_bufs);
            Py_XDECREF(py_ctx);

            num_of_messages -= message->nbuf;

            // Free dynamically allocated memory
            free(message->data); // Free the buffer pointers array
            free(message);       // Free the message itself
        }
    }

    PyGILState_Release(gstate);
}

static PyObject*
helper_jbpf_handle_out_bufs(PyObject* self, PyObject* args)
{
    PyObject* cb;
    int num_of_messages;
    int timeout;

    // parse the arguments
    if (!PyArg_ParseTuple(args, "Oii", &cb, &num_of_messages, &timeout)) {
        PyErr_SetString(PyExc_TypeError, "Expected arguments: cb (callable), num_of_messages (int), timeout (int)");
        return Py_BuildValue("i", 1);
    }

    if (!PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "Expected a callable object (failed PyCallable_Check)");
        return Py_BuildValue("i", 2);
    }

    // Store the Python callback
    Py_XINCREF(cb);

    // Call the emulator with the C wrapper, not the Python object
    c_io_output_handler_wrapper(cb, num_of_messages, timeout);

    // Cleanup
    Py_XDECREF(cb);

    return Py_BuildValue("i", 0);
}

static PyObject*
helper_jbpf_send_input_msg(PyObject* self, PyObject* args)
{
    char* stream_id_data;
    Py_ssize_t stream_id_size;
    char* data;
    Py_ssize_t data_size;
    int size;

    // Parse the stream_id as bytes, data as bytes, and size as an int
    if (!PyArg_ParseTuple(args, "y#y#i", &stream_id_data, &stream_id_size, &data, &data_size, &size)) {
        PyErr_SetString(PyExc_TypeError, "Expected arguments: stream_id (bytes), data (bytes), size (int)");
        return NULL;
    }

    // Ensure stream_id is the expected size for jbpf_io_stream_id_t (e.g., 16 bytes)
    if (stream_id_size != sizeof(jbpf_io_stream_id_t)) {
        PyErr_SetString(PyExc_ValueError, "Invalid stream_id size");
        return NULL;
    }

    // Cast the stream_id_data to a jbpf_io_stream_id_t pointer
    jbpf_io_stream_id_t* stream_id = (jbpf_io_stream_id_t*)stream_id_data;

    // Call the original C function
    int result = jbpf_send_input_msg(stream_id, (void*)data, (size_t)size);

    // Return the result as a Python integer
    return Py_BuildValue("i", result);
}

static PyObject*
helper_jbpf_register_thread(PyObject* self, PyObject* args)
{
    jbpf_register_thread();
    return Py_BuildValue("i", 0);
}

static PyObject*
helper_jbpf_cleanup_thread(PyObject* self, PyObject* args)
{
    jbpf_cleanup_thread();
    return Py_BuildValue("i", 0);
}

static PyObject*
helper_add_time_event_wrapper(PyObject* self, PyObject* args)
{
    uint64_t time;

    if (!PyArg_ParseTuple(args, "K", &time)) {
        PyErr_SetString(PyExc_TypeError, "expecting a uint64_t");
        return Py_BuildValue("i", 1);
    }

    int ret = add_time_event(time);
    return Py_BuildValue("i", ret);
}

static PyObject*
helper_get_time_event_wrapper(PyObject* self, PyObject* args)
{
    uint64_t ts = get_time_event();
    return Py_BuildValue("K", ts);
}

static PyObject*
helper_clear_time_event_wrapper(PyObject* self, PyObject* args)
{
    int ans = 0;
    while (!TAILQ_EMPTY(&head)) {
        struct time_event* elem;
        elem = head.tqh_first;
        ans++;
        TAILQ_REMOVE(&head, head.tqh_first, entries);
        free(elem);
    }
    return Py_BuildValue("i", ans);
}

static PyMethodDef jbpfHelperMethods[] = {
    {"jbpf_write_xran_samples",
     helper_jbpf_write_xran_samples,
     METH_VARARGS,
     "Add sample data in the emulated xran sample queue"},
    {"jbpf_clear_xran_samples",
     helper_jbpf_clear_xran_samples,
     METH_VARARGS,
     "Reset sample data in the emulated xran sample queue"},
    {"jbpf_get_xran_samples",
     helper_jbpf_get_xran_samples,
     METH_VARARGS,
     "Get sample data in the emulated xran sample queue"},
    {"jbpf_add_time_event", helper_add_time_event_wrapper, METH_VARARGS, "Add time events in the emulated time queue"},
    {"jbpf_get_time_event", helper_get_time_event_wrapper, METH_VARARGS, "Get time events in the emulated time queue"},
    {"jbpf_clear_time_events",
     helper_clear_time_event_wrapper,
     METH_VARARGS,
     "Reset time events in the emulated time queue"},
    {"jbpf_codeletset_load", helper_jbpf_load_codeletset, METH_VARARGS, "Load a codeletset"},
    {"jbpf_codeletset_unload", helper_jbpf_unload_codeletset, METH_VARARGS, "Unload a codeletset"},
    {"jbpf_send_input_msg", helper_jbpf_send_input_msg, METH_VARARGS, "Send input message"},
    {"jbpf_handle_out_bufs", helper_jbpf_handle_out_bufs, METH_VARARGS, "Handle output buffers"},
    {"jbpf_register_thread", helper_jbpf_register_thread, METH_VARARGS, "Register thread"},
    {"jbpf_cleanup_thread", helper_jbpf_cleanup_thread, METH_VARARGS, "Cleanup thread"},
    {NULL, NULL, 0, NULL}};

static PyModuleDef jbpfHelperModule = {
    PyModuleDef_HEAD_INIT, "helper_functions", NULL, -1, jbpfHelperMethods, NULL, NULL, NULL, NULL};

static PyObject*
PyInit_jbpf_helpers(void)
{
    return PyModule_Create(&jbpfHelperModule);
}

static bool
run_benchmark_test(char* dir, char* python_file)
{
    PyObject* pName;
    PyObject* sysPath;
    PyObject* pyDir;
    PyObject* pyTempPath;
    bool ok = true;

    sysPath = PySys_GetObject((char*)"path");

    // Append the dir to sys.path
    pyDir = PyUnicode_FromString(dir);
    if (pyDir) {
        if (PyList_Append(sysPath, pyDir) == -1) {
            jbpf_logger(JBPF_ERROR, "Error: Failed to append %s to sys.path\n", dir);
            ok = false;
        }
        Py_DECREF(pyDir); // Decrement reference count
    }

    char* jbpf_path = getenv("JBPF_PATH");
    jbpf_logger(JBPF_INFO, "JBPF_PATH is %s\n", jbpf_path);

    char tempPath[255];
    sprintf(tempPath, "%s/out/", jbpf_path); // Make sure to format first
    pyTempPath = PyUnicode_FromString(tempPath);
    if (pyTempPath) {
        if (PyList_Append(sysPath, pyTempPath) == -1) {
            jbpf_logger(JBPF_ERROR, "Error: Failed to append %s to sys.path\n", tempPath);
            ok = false;
        }
        Py_DECREF(pyTempPath); // Decrement reference count
    }

    pName = PyUnicode_DecodeFSDefault(python_file);
    if (pName) {
        PyObject* pModule = PyImport_Import(pName);
        if (!pModule) {
            ok = false;
            PyErr_Print(); // Print error if module import fails
        }
        Py_DECREF(pName); // Decrement reference count
    }

    return ok;
}

bool
checkFileExists(const char* path)
{
    FILE* file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

static void
print_list_of_hooks()
{
    int num_elems = sizeof(jbpfHookMethods) / sizeof(struct PyMethodDef);

    jbpf_logger(JBPF_INFO, "Available hooks @jbpf_hooks: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", jbpfHookMethods[i].ml_name, jbpfHookMethods[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");

    num_elems = sizeof(jbpfAgentHookMethods) / sizeof(struct PyMethodDef);

    jbpf_logger(JBPF_INFO, "Available hooks @jbpf_agent_hooks: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", jbpfAgentHookMethods[i].ml_name, jbpfAgentHookMethods[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");
}

static void
print_list_of_helper_functions()
{
    int num_elems = sizeof(HelperFunctions) / sizeof(struct PyMethodDef);

    jbpf_logger(JBPF_INFO, "Available Custom Helper Functions: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", HelperFunctions[i].ml_name, HelperFunctions[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");

    num_elems = sizeof(jbpfHelperMethods) / sizeof(struct PyMethodDef);

    jbpf_logger(JBPF_INFO, "Available Helper Functions @jbpf_helpers: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", jbpfHelperMethods[i].ml_name, jbpfHelperMethods[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");
}

void
cleanup()
{
    // Free the memory allocated for the messages
    while (!message_queue.empty()) {
        message_t* message = message_queue.front();
        message_queue.pop();
        free(message->data);
        free(message);
    }

    // Free time events
    while (!TAILQ_EMPTY(&head)) {
        struct time_event* elem;
        elem = head.tqh_first;
        TAILQ_REMOVE(&head, head.tqh_first, entries);
        free(elem);
    }

    // Free sample events
    while (!TAILQ_EMPTY(&sample_head)) {
        struct sample_event* elem;
        elem = sample_head.tqh_first;
        TAILQ_REMOVE(&sample_head, sample_head.tqh_first, entries);
        free(elem->data);
        free(elem);
    }
}

int
main(int argc, char** argv)
{
    jbpf_logger(JBPF_INFO, "Starting JBPF Emulator Test (CPP) ...\n");
    if (argc != 3) {
        jbpf_logger(JBPF_INFO, "Usage: %s /path/to/tests test\n", argv[0]);
        jbpf_logger(JBPF_INFO, "Example: %s $JBPF_PATH/out/emulator/test test_1\n", argv[0]);
        print_list_of_hooks();
        print_list_of_helper_functions();
        return 1;
    }

    // init the queues
    TAILQ_INIT(&head);
    TAILQ_INIT(&sample_head);

    char* path = argv[1];
    char* python_module = argv[2];

    jbpf_logger(JBPF_INFO, "Loading the emulator %s/%s\n", path, python_module);

    // check if the path exists
    char tempPath[255];
    sprintf(tempPath, "%s/%s.py", path, python_module);
    if (!checkFileExists(tempPath)) {
        jbpf_logger(JBPF_ERROR, "File %s does not exist\n", tempPath);
        return 1;
    }

    struct jbpf_config config = {0};
    jbpf_set_default_config_options(&config);
    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);
    jbpf_register_thread();

    jbpf_register_io_output_cb(io_channel_check_output);

    PyImport_AppendInittab("jbpf_hooks", &PyInit_jbpf_hooks);
    PyImport_AppendInittab("jbpf_agent_hooks", &PyInit_jbpf_agent_hooks);
    PyImport_AppendInittab("jbpf_helpers", &PyInit_jbpf_helpers);
    PyImport_AppendInittab("helper_functions", &PyInit_helper_functions);

    Py_Initialize();

    bool res = run_benchmark_test(path, python_module);

    jbpf_stop();
    if (res) {
        jbpf_logger(JBPF_INFO, "Test %s completed successfully.\n", tempPath);
    } else {
        jbpf_logger(JBPF_ERROR, "Test %s failed.\n", tempPath);
    }

    // cleanup
    Py_Finalize();
    cleanup();
    return res ? 0 : 1;
}
