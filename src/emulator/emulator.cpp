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

#define PY_SSIZE_T_CLEAN
#undef _GNU_SOURCE
#include <Python.h>

using namespace std;

// user input
#include "test/hooks.h"
#include "test/helper_functions.hpp"

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

static PyObject*
PyInit_jbpf_hooks(void)
{
    return PyModule_Create(&jbpfHookModule);
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
                fprintf(stderr, "Error: Python callback failed\n");
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

static PyMethodDef jbpfHelperMethods[] = {
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

static void
run_benchmark_test(char* dir, char* python_file)
{
    PyObject* pName;

    PyObject* sysPath = PySys_GetObject((char*)"path");
    PyList_Append(sysPath, PyUnicode_FromString(dir));

    char* jbpf_path = getenv("JBPF_PATH");
    jbpf_logger(JBPF_INFO, "JBPF_PATH is %s\n", jbpf_path);

    char tempPath[255];
    PyList_Append(sysPath, PyUnicode_FromString(tempPath));
    sprintf(tempPath, "%s/out/", jbpf_path);
    PyList_Append(sysPath, PyUnicode_FromString(tempPath));
    pName = PyUnicode_DecodeFSDefault(python_file);
    PyImport_Import(pName);
    PyErr_Print();
    Py_DECREF(pName);
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

    jbpf_logger(JBPF_INFO, "Available hooks: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", jbpfHookMethods[i].ml_name, jbpfHookMethods[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");
}

static void
print_list_of_helper_functions()
{
    int num_elems = sizeof(HelperFunctions) / sizeof(struct PyMethodDef);

    jbpf_logger(JBPF_INFO, "Available Helper Functions: \n");

    // Skip the last element since it is NULL
    for (int i = 0; i < num_elems - 1; i++) {
        jbpf_logger(JBPF_INFO, "* %s : %s\n", HelperFunctions[i].ml_name, HelperFunctions[i].ml_doc);
    }
    jbpf_logger(JBPF_INFO, "\n");
}

int
main(int argc, char** argv)
{
    cout << "Emulator Test (CPP)" << endl;
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " /path/to/tests test" << endl;
        cout << "Example: " << argv[0] << " $JBPF_PATH/out/emulator/test test_1" << endl;
        print_list_of_hooks();
        print_list_of_helper_functions();
        return 1;
    }
    char* path = argv[1];
    char* python_module = argv[2];

    printf("Loading the emulator %s/%s\n", path, python_module);

    // check if the path exists
    char tempPath[255];
    sprintf(tempPath, "%s/%s.py", path, python_module);
    if (!checkFileExists(tempPath)) {
        cout << "File " << tempPath << " does not exist" << endl;
        return 1;
    }

    struct jbpf_config config = {0};
    jbpf_set_default_config_options(&config);
    config.lcm_ipc_config.has_lcm_ipc_thread = false;

    assert(jbpf_init(&config) == 0);
    jbpf_register_thread();

    jbpf_register_io_output_cb(io_channel_check_output);

    PyImport_AppendInittab("jbpf_hooks", &PyInit_jbpf_hooks);
    PyImport_AppendInittab("jbpf_helpers", &PyInit_jbpf_helpers);
    PyImport_AppendInittab("helper_functions", &PyInit_helper_functions);

    Py_Initialize();

    run_benchmark_test(path, python_module);

    jbpf_stop();
    cout << "Test " << tempPath << " completed successfully." << endl;
    return 0;
}