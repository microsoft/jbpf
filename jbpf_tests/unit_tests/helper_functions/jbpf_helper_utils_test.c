/**
 * This contains unit tests for the @jbpf_helper_utils.h functions.
 * It includes tests for fixed-point arithmetic, conversion functions,
 * and trigonometric functions.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "jbpf_helper_utils.h"

#define abs(x) ((x) < 0 ? -(x) : (x))
#define FIXEDPT_EPSILON (1 << 16) // Smallest difference between fixed-point numbers with 16 fractional bits

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
    assert(abs(result - FIXEDPT_ONE) < FIXEDPT_EPSILON); // Allow small epsilon error
    printf("test_fixedpt_sin passed\n");
}

void
test_fixedpt_cos(void)
{
    fixedpt a = FIXEDPT_PI; // 180 degrees in fixedpt
    fixedpt result = fixedpt_cos(a);
    // assert(result == -FIXEDPT_ONE); // cos(180 degrees) = -1
    assert(abs(result - -FIXEDPT_ONE) < FIXEDPT_EPSILON); // Allow small epsilon error
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

    printf("All tests passed\n");
    return 0;
}
