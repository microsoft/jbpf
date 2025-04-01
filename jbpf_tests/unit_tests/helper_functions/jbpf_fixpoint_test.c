// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * Unit Tests for fixed point helper functions
 * This test the following scenarios
 * 1. Conversion from double to fixedpt and back
 * 2. Conversion from uint to fixedpt and back
 * 3. Conversion from fixedpt to double
 * 4. Conversion from fixedpt to uint
 */
#include "jbpf_helper_utils.h"
#include "jbpf_fixpoint_utils.h"
#include <assert.h>
#include <stdio.h>

void
test_fixedpt_from_double(void)
{
    double value = 3.75;
    fixedpt result = fixedpt_from_double(value);
    double converted_back = fixedpt_to_double(result);
    assert(converted_back == value);
    printf("test_fixedpt_from_double passed.\n");
}

void
test_fixedpt_to_double(void)
{
    fixedpt value = fixedpt_from_double(5.5);
    double result = fixedpt_to_double(value);
    assert(result == 5.5);
    printf("test_fixedpt_to_double passed.\n");
}

void
test_fixedpt_from_uint(void)
{
    unsigned int value = 10;
    fixedpt result = fixedpt_from_uint(value);
    unsigned int converted_back = fixedpt_to_uint(result);
    assert(converted_back == value);
    printf("test_fixedpt_from_uint passed.\n");
}

void
test_fixedpt_to_uint(void)
{
    fixedpt value = fixedpt_from_uint(20);
    unsigned int result = fixedpt_to_uint(value);
    assert(result == 20);
    printf("test_fixedpt_to_uint passed.\n");
}

void
test_fixedpt_from_double_approx(void)
{
    assert(fixedpt_from_double_approx(1, FIXEDPT_ONE) == (1 * FIXEDPT_ONE));
    assert(fixedpt_from_double_approx(3, FIXEDPT_ONE) == (3 * FIXEDPT_ONE));
    assert(fixedpt_from_double_approx(-2, FIXEDPT_ONE) == (-2 * FIXEDPT_ONE));
    printf("test_fixedpt_from_double_approx passed!\n");
}

void
test_fixedpt_to_int_approx(void)
{
    assert(fixedpt_to_int_approx(1 * FIXEDPT_ONE, FIXEDPT_ONE) == 1);
    assert(fixedpt_to_int_approx(3 * FIXEDPT_ONE, FIXEDPT_ONE) == 3);
    assert(fixedpt_to_int_approx(-2 * FIXEDPT_ONE, FIXEDPT_ONE) == -2);
    printf("test_fixedpt_to_int_approx passed!\n");
}
int
main(int argc, char* argv[])
{
    // Run all tests
    test_fixedpt_from_double();
    test_fixedpt_to_double();
    test_fixedpt_from_uint();
    test_fixedpt_to_uint();
    test_fixedpt_from_double_approx();
    test_fixedpt_to_int_approx();
    printf("All tests passed!\n");
    return 0;
}
