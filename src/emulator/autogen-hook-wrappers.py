
## this script is used to generate c code for jbpf hooks
## this script reads the C header files and then generates the wrapper functions for Jbpf Emulator
## For example:
## python3 autogen-hook-wrappers.py /jbpf/api/jbpf_hooks1.h /jbpf/api/jbpf_hooks2.h | tee generated-hooks.h

import re
import sys

def remove_c_comments(code):
    pattern = r'/\*.*?\*/'
    return re.sub(pattern, '', code, flags=re.DOTALL)

## check if given is a C primitive type
## for example, char or char*
## void is categorized as a primitive type
def is_c_primitive_type(s):
    s = s.replace(" ", "")
    c_primitive_types = (
        'char', 'unsigned char', 'signed char',
        'int', 'unsigned int', 'signed int',
        'short', 'unsigned short', 'signed short',
        'long', 'unsigned long', 'signed long',
        'float', 'double', 'long double',
        'int8_t', 'int16_t', 'int32_t', 'int64_t',
        'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
        'void'
    )
    return s in c_primitive_types or s.replace("*", "") in c_primitive_types

## map the C type to the Python type used by PyArg_ParseTuple
## see https://docs.python.org/3/c-api/arg.html
def map_c_type_to_python_format(c_type):
    c_type = c_type.strip()
    if c_type.endswith("*"):
        return "Y"
    mapping = {
        'uint16_t': 'H',
        'uint32_t': 'I',
        'uint64_t': 'Q',
        'int16_t': 'h',
        'char': 'c',
        'uint8_t': 'B',
        'signed char': 'b',
        'unsigned char': 'B',
        'bool': '?',
        'short': 'h',
        'unsigned short': 'H',
        'int': 'i',
        'unsigned int': 'I',
        'long': 'l',
        'unsigned long': 'L',
        'long long': 'q',
        'unsigned long long': 'Q',
        'float': 'f',
        'double': 'd',
        'char[]': 's'
    }
    ans = mapping.get(c_type, '')
    if not ans:
        raise Exception(f"unknown c type: {c_type}")
    return ans

## should we check size
## No for fapi hooks - because fapi structs have variable sized array
## No for primitive types e.g. int16_t
## Yes for other hooks
def check_size(hook_name, struct_type):
    if hook_name.startswith("fapi_"):
        return False
    if (not struct_type) or is_c_primitive_type(struct_type):
        return False
    ## we should check the size match for a struct type
    return True

def write_a_c_function(hook_name, args_arr, define_hook):
    s = ""
    declared_hooks_already = ("report_stats", "periodic_call")
    if hook_name not in declared_hooks_already:
        s += f"{define_hook}({hook_name})\n"
    s += f"static PyObject* hook_{hook_name}_wrapper(PyObject *self, PyObject *args)" + "{\n"
    args_format = ""
    meet_struct = False
    struct_type = None
    arr = []
    for (arg_type, arg_name) in args_arr:
        arg_type = arg_type.strip() 
        s += f"{arg_type} {arg_name};\n"  
        if is_c_primitive_type(arg_type):
            args_format += map_c_type_to_python_format(arg_type)
            arr.append(f"&{arg_name}")
        else:
            meet_struct = True
            struct_type = arg_type.replace("*", "").strip()
            args_format += "y#"
            arr.append(f"&{arg_name.replace('*', '')}")
            arr.append(f"&size")
            s += "size_t size;\n"
    s += f"if (!PyArg_ParseTuple(args, \"{args_format}\", {', '.join(arr)}))" + "{\n"
    s += "PyErr_SetString(PyExc_TypeError, \"wrong argument type\");\n"
    s += "return Py_BuildValue(\"i\", 1);\n"
    s += "}\n"
    ## if we have a struct, we need to check the size of the buffer
    ## except for fapi hooks - because fapi structs have variable sized array
    ## or when the struct_type is actually a primitive type e.g. int16_t
    if meet_struct and check_size(hook_name, struct_type):
        s += f"if (size != sizeof({struct_type}))" + "{\n"
        s += "PyErr_SetString(PyExc_TypeError, \"wrong buffer size\");\n"
        s += "return Py_BuildValue(\"i\", 2);\n"
        s += "}\n"
    control_hook = ""
    if define_hook == "DEFINE_JBPF_CTRL_HOOK":
        control_hook = "ctrl_"
    s += f"{control_hook}hook_{hook_name}({', '.join([arg_name.replace('*', '') for (_, arg_name) in args_arr])});\n"
    s += "return Py_BuildValue(\"i\", 0);\n"
    s += "}\n"
    return s

def process_header_file(input_file):
    hooks_arr = []
    with open(input_file, 'r') as f:
        data = remove_c_comments(f.read())

    def process_hook(hook, data, define_hook):        
        hooks = re.findall(rf'{re.escape(hook)}\((.*?)\)', data, re.DOTALL)

        for data in hooks:
            hook = data[:data.find(',')].strip()
            hook_proto = data[data.find('HOOK_PROTO(') + len("HOOK_PROTO("):].strip().split(',')
            hook_proto = [x.strip() for x in hook_proto]
            parameters = []
            for param in hook_proto:
                param = param.strip()
                ## remove multiple spaces
                while '  ' in param:
                    param = param.replace('  ', ' ')
                if param == '':
                    continue
                param = param.split(' ')
                param = [x.strip() for x in param]
                if len(param) == 2:
                    parameters.append((param[0], param[1]))
                else:
                    parameters.append((param[0] + " " + param[1], param[2]))
            print(write_a_c_function(hook, parameters, define_hook))
            hooks_arr.append(hook)

    process_hook("DECLARE_JBPF_HOOK", data, "DEFINE_JBPF_HOOK")
    process_hook("DECLARE_JBPF_CTRL_HOOK", data, "DEFINE_JBPF_CTRL_HOOK")

    return hooks_arr

if __name__ == '__main__':
    hooks = []
    for i in range(1, len(sys.argv)):
        input_file = sys.argv[i]
        hooks.extend(process_header_file(input_file))
    print("static PyMethodDef jbpfHookMethods[] = {")
    for hook in hooks:
        print('{"hook_' + hook + '", hook_' + hook + '_wrapper, METH_VARARGS, "' + hook + '"},')

    # ## add the two special hooks
    print("{NULL, NULL, 0, NULL}")
    print("};")
