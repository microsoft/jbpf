#include "jbpf.h"
#include "jbpf_hook.h"
#include "jbpf_agent_common.h"
#include "jbpf_defs.h"
#include "../hook_wrapper.h"
#include "Python.h"

// auto generated hook wrappers using autogen-hook-wrappers.py
#include "auto_generated_hooks_wrapper.hpp"

///////////////////////////////////////////////////////////////////////////////////
// The following code is a minimal working example to define a hook and wrapper. //
///////////////////////////////////////////////////////////////////////////////////

// DECLARE_JBPF_HOOK(
//     test1,
//     struct jbpf_generic_ctx ctx,
//     ctx,
//     HOOK_PROTO(struct packet* p, int ctx_id),
//     HOOK_ASSIGN(ctx.ctx_id = ctx_id; ctx.data = (uint64_t)(void*)p; ctx.data_end = (uint64_t)(void*)(p + 1);))

// DEFINE_HOOK_AND_WRAPPER(test1, packet, "y#i", int, ctx_id)

// Unrolled version of the above #L14

// DEFINE_JBPF_HOOK(test1)

// static PyObject*
// hook_test1_wrapper(PyObject* self, PyObject* args)
// {
//     struct packet* p;
//     size_t size;
//     int ctx_id;
//     if (!PyArg_ParseTuple(args, "y#i", &p, &size, &ctx_id)) {
//         PyErr_SetString(PyExc_TypeError, "wrong argument type");
//         return Py_BuildValue("i", 1);
//     }
//     if (size != sizeof(struct packet)) {
//         PyErr_SetString(PyExc_TypeError, "wrong buffer size");
//         return Py_BuildValue("i", 2);
//     }
//     hook_test1(p, ctx_id);
//     return Py_BuildValue("i", 0);
// }

// user input
// static PyMethodDef jbpfHookMethods[] = {
//     {"hook_test1", hook_test1_wrapper, METH_VARARGS, "test1"}, {NULL, NULL, 0, NULL}};