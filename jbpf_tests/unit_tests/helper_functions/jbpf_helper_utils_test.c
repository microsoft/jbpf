// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This contains unit tests for the jbpf_helper_utils functions.
 *
 * The functions tested include:
 * - float_to_fixed
 * - fixed_to_float
 * - double_to_fixed
 * - fixed_to_double
 * - fixedpt_add
 * - fixedpt_sub
 * - fixedpt_mul
 * - fixedpt_div
 * - fixedpt_fracpart
 * - fixedpt_str
 * - fixedpt_cstr
 * - fixedpt_sqrt
 * - fixedpt_sin
 * - fixedpt_cos
 * - fixedpt_tan
 * - fixedpt_pow
 * - fixedpt_exp
 * - fixedpt_ln
 * - fixedpt_sqrt
 * - fixedpt_fromint
 * - fixedpt_toint
 * - fixedpt_log
 * - fixedpt_abs
 * - fixedpt_fixed_to_float
 * - fixedpt_fixed_to_double
 * - fixedpt_float_to_fixed
 * - fixedpt_double_to_fixed
 * - fixedpt_convert_float_to_fixed_and_back
 * - fixedpt_convert_double_to_fixed_and_back
 * - fixedpt_convert_fixed_to_float_and_back
 * - fixedpt_convert_fixed_to_double_and_back
 */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "jbpf_helper_utils.h"

void
test_fixedpt_fromint(void)
{
    fixedpt result = fixedpt_fromint(5);
    assert(result == (5 << FIXEDPT_FBITS)); // 5 shifted left by FIXEDPT_FBITS
    printf("test_fixedpt_fromint passed\n");
}

void
test_fixedpt_toint(void)
{
    fixedpt input = (3 << FIXEDPT_FBITS) + (1 << (FIXEDPT_FBITS - 1)); // Should round down to 3
    int result = fixedpt_toint(input);
    assert(result == 3);
    printf("test_fixedpt_toint passed\n");
}

void
test_fixedpt_add(void)
{
    fixedpt a = 5 << FIXEDPT_FBITS;
    fixedpt b = 3 << FIXEDPT_FBITS;
    fixedpt result = fixedpt_add(a, b);
    assert(result == 8 << FIXEDPT_FBITS);
    printf("test_fixedpt_add passed\n");
}

void
test_fixedpt_sub(void)
{
    fixedpt a = 5 << FIXEDPT_FBITS;
    fixedpt b = 3 << FIXEDPT_FBITS;
    fixedpt result = fixedpt_sub(a, b);
    assert(result == 2 << FIXEDPT_FBITS);
    printf("test_fixedpt_sub passed\n");
}

void
test_fixedpt_mul(void)
{
    fixedpt a = 5 << FIXEDPT_FBITS;
    fixedpt b = 3 << FIXEDPT_FBITS;
    fixedpt result = fixedpt_mul(a, b);
    assert(result == 15 << FIXEDPT_FBITS); // 5 * 3 = 15, scaled by FIXEDPT_FBITS
    printf("test_fixedpt_mul passed\n");
}

void
test_fixedpt_div(void)
{
    fixedpt a = 6 << FIXEDPT_FBITS;
    fixedpt b = 3 << FIXEDPT_FBITS;
    fixedpt result = fixedpt_div(a, b);
    assert(result == 2 << FIXEDPT_FBITS); // 6 / 3 = 2, scaled by FIXEDPT_FBITS
    printf("test_fixedpt_div passed\n");
}

void
test_fixedpt_fracpart(void)
{
    fixedpt a = (5 << FIXEDPT_FBITS) + (1 << (FIXEDPT_FBITS - 2)); // a = 5.25
    fixedpt frac = fixedpt_fracpart(a);
    assert(frac == (1 << (FIXEDPT_FBITS - 2))); // Fraction part should be 0.25
    printf("test_fixedpt_fracpart passed\n");
}

void
test_fixedpt_str(void)
{
    fixedpt a = (5 << FIXEDPT_FBITS) + (1 << (FIXEDPT_FBITS - 2)); // a = 5.25
    char str[25];
    fixedpt_str(a, str, -1); // Default number of decimal places
    assert(strcmp(str, "5.25") == 0);
    printf("test_fixedpt_str passed\n");
}

void
test_fixedpt_cstr(void)
{
    fixedpt a = (5 << FIXEDPT_FBITS) + (1 << (FIXEDPT_FBITS - 2)); // a = 5.25
    char* result = fixedpt_cstr(a, -1);
    assert(strcmp(result, "5.25") == 0);
    printf("test_fixedpt_cstr passed\n");
}

void
test_fixedpt_sqrt(void)
{
    fixedpt a = (4 << FIXEDPT_FBITS); // 4 in fixedpt
    fixedpt result = fixedpt_sqrt(a);
    assert(result == (2 << FIXEDPT_FBITS)); // sqrt(4) = 2
    printf("test_fixedpt_sqrt passed\n");
}

void
test_fixedpt_sin(void)
{
    fixedpt a = FIXEDPT_PI / 2; // 90 degrees in fixedpt
    fixedpt result = fixedpt_sin(a);
    // assert(result == FIXEDPT_ONE); // sin(90 degrees) = 1
    assert(fixedpt_abs(result - FIXEDPT_ONE) < FIXEDPT_EPSILON); // Allow small epsilon error
    printf("test_fixedpt_sin passed\n");
}

void
test_fixedpt_cos(void)
{
    fixedpt a = FIXEDPT_PI; // 180 degrees in fixedpt
    fixedpt result = fixedpt_cos(a);
    // assert(result == -FIXEDPT_ONE); // cos(180 degrees) = -1
    assert(fixedpt_abs(result - -FIXEDPT_ONE) < FIXEDPT_EPSILON); // Allow small epsilon error
    printf("test_fixedpt_cos passed\n");
}

void
test_fixedpt_tan(void)
{
    fixedpt a = FIXEDPT_PI / 4; // 45 degrees in fixedpt
    fixedpt result = fixedpt_tan(a);
    assert(result == FIXEDPT_ONE); // tan(45 degrees) = 1
    printf("test_fixedpt_tan passed\n");
}

void
test_fixedpt_exp(void)
{
    fixedpt a = fixedpt_rconst(1); // e^1
    fixedpt result = fixedpt_exp(a);
    assert(result == fixedpt_rconst(2.7182818284590452354)); // Approximation of e
    printf("test_fixedpt_exp passed\n");
}

void
test_fixedpt_ln(void)
{
    fixedpt a = fixedpt_rconst(2.7182818284590452354); // ln(e)
    fixedpt result = fixedpt_ln(a);
    assert(result == fixedpt_rconst(1)); // ln(e) = 1
    printf("test_fixedpt_ln passed\n");
}

void
test_fixedpt_log(void)
{
    fixedpt a = fixedpt_rconst(8); // log2(8)
    fixedpt base = fixedpt_rconst(2);
    fixedpt result = fixedpt_log(a, base);
    // assert(result == fixedpt_rconst(3)); // log2(8) = 3
    assert(fixedpt_abs(result - fixedpt_rconst(3)) < FIXEDPT_EPSILON); // Allow small epsilon error
    printf("test_fixedpt_log passed\n");
}

void
test_fixedpt_abs(void)
{
    fixedpt a = fixedpt_rconst(-5); // -5 in fixedpt
    fixedpt result = fixedpt_abs(a);
    assert(result == fixedpt_rconst(5)); // Absolute value should be 5
    fixedpt b = fixedpt_rconst(3);       // 3 in fixedpt
    fixedpt result2 = fixedpt_abs(b);
    assert(result2 == fixedpt_rconst(3)); // Absolute value should be 3
    printf("test_fixedpt_abs passed\n");
}

void
test_fixedpt_pow(void)
{
    fixedpt base = fixedpt_rconst(2); // 2 in fixedpt
    fixedpt exp = fixedpt_rconst(3);  // 3 in fixedpt
    fixedpt result = fixedpt_pow(base, exp);
    // allow small epsilon error
    assert(fixedpt_abs(result - fixedpt_rconst(8)) < FIXEDPT_EPSILON); // 2^3 = 8
    // assert(result == fixedpt_rconst(8)); // 2^3 = 8
    printf("test_fixedpt_pow passed\n");
}

void
test_fixed_to_float(void)
{
    fixedptd a = fixedpt_rconst(5.25); // 5.25 in fixedpt
    float result = fixed_to_float(a);
    assert(fabs(result - 5.25) < 0.0001); // Allow small epsilon error
    printf("test_fixed_to_float passed\n");
}

void
test_fixed_to_double(void)
{
    fixedptd a = fixedpt_rconst(5.25); // 5.25 in fixedpt
    double result = fixed_to_double(a);
    assert(fabs(result - 5.25) < 0.0001); // Allow small epsilon error
    printf("test_fixed_to_double passed\n");
}

void
test_float_to_fixed(void)
{
    float a = 5.25; // 5.25 in float
    fixedpt result = float_to_fixed(a);
    assert(result == fixedpt_rconst(5.25)); // Should be equal to fixedpt representation of 5.25
    printf("test_float_to_fixed passed\n");
}

void
test_double_to_fixed(void)
{
    double a = 5.25; // 5.25 in double
    fixedpt result = double_to_fixed(a);
    assert(result == fixedpt_rconst(5.25)); // Should be equal to fixedpt representation of 5.25
    printf("test_double_to_fixed passed\n");
}

void
test_convert_float_to_fixed_and_back(void)
{
    float a = 5.25; // 5.25 in float
    fixedpt fixed_value = float_to_fixed(a);
    float result = fixed_to_float(fixed_value);
    assert(fabs(result - a) < 0.0001); // Allow small epsilon error
    printf("test_convert_float_to_fixed_and_back passed\n");
}

void
test_convert_double_to_fixed_and_back(void)
{
    double a = 5.25; // 5.25 in double
    fixedpt fixed_value = double_to_fixed(a);
    double result = fixed_to_double(fixed_value);
    assert(fabs(result - a) < 0.0001); // Allow small epsilon error
    printf("test_convert_double_to_fixed_and_back passed\n");
}

void
test_convert_fixed_to_float_and_back(void)
{
    fixedpt a = fixedpt_rconst(5.25); // 5.25 in fixedpt
    float float_value = fixed_to_float(a);
    fixedpt result = float_to_fixed(float_value);
    assert(result == a); // Should be equal to original fixedpt value
    printf("test_convert_fixed_to_float_and_back passed\n");
}

void
test_convert_fixed_to_double_and_back(void)
{
    fixedpt a = fixedpt_rconst(5.25); // 5.25 in fixedpt
    double double_value = fixed_to_double(a);
    fixedpt result = double_to_fixed(double_value);
    assert(result == a); // Should be equal to original fixedpt value
    printf("test_convert_fixed_to_double_and_back passed\n");
}

int
main(void)
{
    test_fixedpt_fromint();
    test_fixedpt_toint();
    test_fixedpt_add();
    test_fixedpt_sub();
    test_fixedpt_mul();
    test_fixedpt_div();
    test_fixedpt_fracpart();
    test_fixedpt_str();
    test_fixedpt_cstr();
    test_fixedpt_sqrt();
    test_fixedpt_sin();
    test_fixedpt_cos();
    test_fixedpt_tan();
    test_fixedpt_exp();
    test_fixedpt_ln();
    test_fixedpt_log();
    test_fixedpt_abs();
    test_fixedpt_pow();
    test_fixed_to_float();
    test_fixed_to_double();
    test_float_to_fixed();
    test_double_to_fixed();
    test_convert_float_to_fixed_and_back();
    test_convert_double_to_fixed_and_back();
    test_convert_fixed_to_float_and_back();
    test_convert_fixed_to_double_and_back();
    printf("All tests passed\n");
    return 0;
}
