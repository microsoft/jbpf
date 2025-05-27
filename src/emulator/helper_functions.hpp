#include <Python.h>
#include <jbpf_helper_api_defs_ext.h>

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

void
custom_init()
{
    // Custom initialization code here ...
    cout << "Custom initialization code here ..." << endl;
}

static int
jbpf_sample_function(int a, int b, int c)
{
    cout << "This is a sample helper function. (" << a << ", " << b << ", " << c << ")" << endl;
    return a + b + c;
}

bool
register_functions()
{
    // TODO:
    cout << "Registering functions here ..." << endl;
    jbpf_helper_func_def_t helper_func = {
        "new_helper_func", CUSTOM_HELPER_START_ID, (jbpf_helper_func_t)jbpf_sample_function};
    helper_func.reloc_id = CUSTOM_HELPER_START_ID;
    assert(jbpf_register_helper_function(helper_func) == 0);
    cout << "Registered functions successfully." << endl;
    return true;
}

static PyMethodDef HelperFunctions[] = {
    {"list_of_hooks", helper_list_of_hooks, METH_VARARGS, "Get a list of available hooks"}, {NULL, NULL, 0, NULL}};
