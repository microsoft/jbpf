// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef _JBPF_HOOK_WRAPPER
#define _JBPF_HOOK_WRAPPER

#define NARGS(...) NARGS_(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define NARGS_(_9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N

#define CONC(A, B) CONC_(A, B)
#define CONC_(A, B) A##B

#define PREFIX_0(E)
#define PREFIX_1(E) &E
#define PREFIX_2(E, ...) &E, PREFIX_1(__VA_ARGS__)
#define PREFIX_3(E, ...) &E, PREFIX_2(__VA_ARGS__)
#define PREFIX_4(E, ...) &E, PREFIX_3(__VA_ARGS__)
#define PREFIX_5(E, ...) &E, PREFIX_4(__VA_ARGS__)
#define PREFIX_6(E, ...) &E, PREFIX_5(__VA_ARGS__)
#define PREFIX_7(E, ...) &E, PREFIX_6(__VA_ARGS__)
#define PREFIX_8(E, ...) &E, PREFIX_7(__VA_ARGS__)
#define PREFIX_9(E, ...) &E, PREFIX_8(__VA_ARGS__)

#define GET_REFERENCE(...) CONC(PREFIX_, NARGS(__VA_ARGS__))(__VA_ARGS__)

#define DEFINE_HOOK_AND_WRAPPER(hook_name, structure_type, args_format, data_type, ...)              \
    DEFINE_JBPF_HOOK(hook_name)                                                                      \
    static PyObject* hook_##hook_name##_wrapper(PyObject* self, PyObject* args)                      \
    {                                                                                                \
        struct structure_type* struct_data;                                                          \
        data_type __VA_ARGS__;                                                                       \
        size_t size;                                                                                 \
        if (!PyArg_ParseTuple(args, args_format, &struct_data, &size, GET_REFERENCE(__VA_ARGS__))) { \
            PyErr_SetString(PyExc_TypeError, "wrong argument type");                                 \
            return Py_BuildValue("i", 1);                                                            \
        }                                                                                            \
        if (size != sizeof(struct structure_type)) {                                                 \
            PyErr_SetString(PyExc_TypeError, "wrong buffer size");                                   \
            return Py_BuildValue("i", 2);                                                            \
        }                                                                                            \
        hook_##hook_name(struct_data, __VA_ARGS__);                                                  \
        return Py_BuildValue("i", 0);                                                                \
    }

#endif