#include <Python.h>

static PyObject*
helper_list_of_hooks(PyObject* self, PyObject* args)
{
    int len = sizeof(jbpfHookMethods) / sizeof(struct PyMethodDef) - 1;
    PyObject* lst = PyList_New(len);
    if (!lst) {
        return NULL;
    }
    for (int i = 0; i < len; i++) {
        PyObject* name = PyUnicode_FromString(jbpfHookMethods[i].ml_name);
        if (!name) {
            Py_DECREF(lst);
            return NULL;
        }
        PyList_SET_ITEM(lst, i, name);
    }
    return lst;
}

static PyMethodDef HelperFunctions[] = {
    {"list_of_hooks", helper_list_of_hooks, METH_VARARGS, "Get a list of available hooks"}, {NULL, NULL, 0, NULL}};
