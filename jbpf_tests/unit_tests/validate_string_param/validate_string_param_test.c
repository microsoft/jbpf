// Copyright (c) Microsoft Corporation. All rights reserved.
#include <assert.h>

#include "jbpf_int.h"
#include "jbpf_test_lib.h"
#include "jbpf_utils.h"

void
test_valid_parameters(void** state)
{
    char param_name[] = "TestParam";
    char param[] = "ValidString";
    JBPF_UNUSED(param);
    JBPF_UNUSED(param_name);
    assert(validate_string_param(param_name, param, 50, NULL) == 1);
}

void
test_param_name_null(void** state)
{
    char param[] = "ValidString";
    JBPF_UNUSED(param);
    assert(validate_string_param(NULL, param, 50, NULL) == -1);
}

void
test_param_null(void** state)
{
    char param_name[] = "TestParam";
    JBPF_UNUSED(param_name);
    assert(validate_string_param(param_name, NULL, 50, NULL) == -1);
}

void
test_param_maxlen_zero(void** state)
{
    char param_name[] = "TestParam";
    char param[] = "ValidString";
    JBPF_UNUSED(param);
    JBPF_UNUSED(param_name);
    assert(validate_string_param(param_name, param, 0, NULL) == -1);
}

void
test_empty_param(void** state)
{
    jbpf_codeletset_load_error_s err = {0};
    char param_name[] = "TestParam";
    char param[] = "";
    JBPF_UNUSED(param);
    JBPF_UNUSED(param_name);
    JBPF_UNUSED(err);
    assert(validate_string_param(param_name, param, 50, &err) == 0);
    assert(strcmp(err.err_msg, "TestParam is not set\n") == 0);
}

void
test_param_exceeds_maxlen(void** state)
{
    jbpf_codeletset_load_error_s err = {0};
    char param_name[] = "TestParam";
    char param[52] = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ";
    JBPF_UNUSED(param);
    JBPF_UNUSED(param_name);
    JBPF_UNUSED(err);
    assert(validate_string_param(param_name, param, 50, &err) == 0);
    assert(strcmp(err.err_msg, "TestParam exceeds maximum length 50\n") == 0);
}

void
test_param_equals_maxlen(void** state)
{
    char param_name[] = "TestParam";
    char param[52] = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXY";
    JBPF_UNUSED(param);
    JBPF_UNUSED(param_name);
    assert(validate_string_param(param_name, param, 52, NULL) == 1);
}

int
main(int argc, char** argv)
{

    const jbpf_test tests[] = {
        JBPF_CREATE_TEST(test_valid_parameters, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_param_name_null, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_param_null, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_param_maxlen_zero, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_empty_param, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_param_exceeds_maxlen, NULL, NULL, NULL),
        JBPF_CREATE_TEST(test_param_equals_maxlen, NULL, NULL, NULL),
    };

    int num_tests = sizeof(tests) / sizeof(jbpf_test);
    return jbpf_run_test(tests, num_tests, NULL, NULL);
}
