// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This contains unit tests for the jbpf_fixpoint_utils functions.
 * The functions tested include:
 * - float_to_fixed
 * - fixed_to_float
 * - double_to_fixed
 * - fixed_to_double
 * - compare_float
 * - compare_double
 * The tests also include basic arithmetic operations such as addition, subtraction,
 * multiplication, and division on fixed-point values.
 */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "jbpf_fixpoint_utils.h"

void
test_float_and_double(void)
{
    // Test 0: Zeros for both float
    {
        uint32_t ieee_float = 0x00000000; // 0.0 in IEEE 754 format
        int32_t fixed_value = float_to_fixed(ieee_float);
        float float_value = fixed_to_float(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_float(float_value, 0.0f, 0.0001f));
        printf("Test 0 passed: float-to-fixed and back to float (0.0)\n");
    }

    // Test 1: Zeros for double
    {
        double value = 0.0;
        int64_t fixed_value = double_to_fixed(value);
        double double_value = fixed_to_double(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_double(double_value, 0.0, 0.0001));
        printf("Test 1 passed: double-to-fixed and back to double (0.0)\n");
    }

    // Test 2: Basic float-to-fixed and back to float conversion
    {
        float value = 1.0;
        int32_t fixed_value = float_to_fixed(value);
        float float_value = fixed_to_float(fixed_value);
        // Allow some small error in floating point comparison
        assert(compare_float(float_value, 1.0, 0.01f));
        printf("Test 2 passed: float-to-fixed and back to float (1.0)\n");
    }

    // Test 3: Convert negative float to fixed and back to float
    {
        float value = -1.0;
        int32_t fixed_value = float_to_fixed(value);
        float float_value = fixed_to_float(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_float(float_value, -1.0f, 0.0001f));
        printf("Test 3 passed: negative float-to-fixed and back to float (-1.0)\n");
    }

    // Test 4: Convert negative double to fixed and back to double
    {
        double value = -1.5;
        int64_t fixed_value = double_to_fixed(value);
        printf("fixed_value = %ld\n", fixed_value);
        print_fixed_double(fixed_value);

        printf("fixed_value = %ld\n", fixed_value);
        double double_value = fixed_to_double(fixed_value);
        printf("double_value = %lf\n", double_value);

        // Allow some small error in floating point comparison
        assert(compare_double(double_value, -1.5, 0.0001));
        printf("Test 4 passed: negative double-to-fixed and back to double (-1.0)\n");
    }

    // Test 5: Complex math: Add two fixed-point values
    {
        float x = 1.0;
        float y = 2.0;
        int32_t fixed1 = float_to_fixed(x);
        int32_t fixed2 = float_to_fixed(y);

        int32_t result_fixed = fixedpt_add(fixed1, fixed2);
        float result_float = fixed_to_float(result_fixed);

        // Expected result: 1.0 + 2.0 = 3.0
        assert(compare_float(result_float, 3.0f, 0.0001f));
        printf("Test 5 passed: Complex math (1.0 + 2.0 = 3.0)\n");
    }

    // Test 6: Complex math: Subtract two fixed-point values
    {
        int32_t fixed1 = float_to_fixed(2.0);
        int32_t fixed2 = float_to_fixed(1.0);

        int32_t result_fixed = fixedpt_sub(fixed1, fixed2);
        float result_float = fixed_to_float(result_fixed);

        // Expected result: 2.0 - 1.0 = 1.0
        assert(compare_float(result_float, 1.0f, 0.0001f));
        printf("Test 6 passed: Complex math (2.0 - 1.0 = 1.0)\n");
    }

    // Test 7: Complex math: Multiply two fixed-point values
    {
        int32_t fixed1 = float_to_fixed(2.0);
        int32_t fixed2 = float_to_fixed(1.0);
        printf("fixed1 = %d, fixed2 = %d\n", fixed1, fixed2);

        int64_t result_fixed = (int64_t)fixed1 * (int64_t)fixed2;
        result_fixed >>= 16; // Adjust result to fit Q16.16 format
        float result_float = fixed_to_float(result_fixed);

        // Expected result: 1.0 * 2.0 = 2.0
        print_fixed_float(result_fixed);
        assert(compare_float(result_float, 2.0f, 0.0001f));
        printf("Test 7 passed: Complex math (1.0 * 2.0 = 2.0)\n");
    }

    // Test 8: Complex math: Divide two fixed-point values
    {
        int32_t fixed1 = float_to_fixed(2.0);
        int32_t fixed2 = float_to_fixed(1.0);

        int64_t result_fixed = (int64_t)fixed1 << 16; // Scale up to avoid loss of precision
        result_fixed /= fixed2;
        float result_float = fixed_to_float((int32_t)result_fixed);

        // Expected result: 2.0 / 1.0 = 2.0
        assert(compare_float(result_float, 2.0f, 0.0001f));
        printf("Test 8 passed: Complex math (2.0 / 1.0 = 2.0)\n");
    }

    // Test 9: Complex math: Add two fixed-point values (double)
    {
        double value1 = 1.0;
        double value2 = 2.0;
        int64_t fixed1 = double_to_fixed(value1);
        int64_t fixed2 = double_to_fixed(value2);

        int64_t result_fixed = fixedpt_add(fixed1, fixed2);
        double result_double = fixed_to_double(result_fixed);

        // Expected result: 1.0 + 2.0 = 3.0
        assert(compare_double(result_double, 3.0, 0.0001));
        printf("Test 9 passed: Complex math (1.0 + 2.0 = 3.0) [double]\n");
    }

    // Test 10: Complex math: Subtract two fixed-point values (double)
    {
        double value1 = 2.0;
        double value2 = 1.0;
        int64_t fixed1 = double_to_fixed(value1);
        int64_t fixed2 = double_to_fixed(value2);

        int64_t result_fixed = fixedpt_sub(fixed1, fixed2);
        double result_double = fixed_to_double(result_fixed);

        // Expected result: 2.0 - 1.0 = 1.0
        assert(compare_double(result_double, 1.0, 0.0001));
        printf("Test 10 passed: Complex math (2.0 - 1.0 = 1.0) [double]\n");
    }

    // Test 11: Complex math: Multiply two fixed-point values (double)
    {
        double value1 = 1.0;
        double value2 = 2.0;
        int64_t fixed1 = double_to_fixed(value1);
        int64_t fixed2 = double_to_fixed(value2);

        // Multiply two Q32.32 fixed-point values, resulting in Q64.64 format
        int64_t result_fixed = ((fixed1 >> 16) * (fixed2 >> 16));

        // Convert the result back to double
        double result_double = fixed_to_double(result_fixed);

        // Expected result: 1.0 * 2.0 = 2.0
        assert(compare_double(result_double, 2.0, 0.0001));
        printf("Test 11 passed: Complex math (1.0 * 2.0 = 2.0) [double]\n");
    }

    // Test 12: Complex math: Divide two fixed-point values (double)
    {
        double value1 = 2.0;
        double value2 = 1.0;
        int64_t fixed1 = double_to_fixed(value1);
        int64_t fixed2 = double_to_fixed(value2);

        // Scale the dividend before division to maintain precision
        int64_t result_fixed = (fixed1 / fixed2) << 32; // Scale fixed1 before dividing by fixed2

        // Convert the result back to double
        double result_double = fixed_to_double(result_fixed);

        // Expected result: 2.0 / 1.0 = 2.0
        assert(compare_double(result_double, 2.0, 0.0001));
        printf("Test 12 passed: Complex math (2.0 / 1.0 = 2.0) [double]\n");
    }

    // Test 13: Convert negative float to fixed and back to float
    {
        int32_t fixed_value = float_to_fixed(-1.0);
        float float_value = fixed_to_float(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_float(float_value, -1.0f, 0.0001f));
        printf("Test 13 passed: negative float-to-fixed and back to float (-1.0)\n");
    }

    // Test 14: fixed_to_float
    {
        int32_t fixed_value = 98304; // 1.5 in Q16.16 format
        float float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, 1.5f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (1.5)\n");

        fixed_value = 65536; // 1.0 in Q16.16 format
        float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, 1.0f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (1.0)\n");

        fixed_value = -65536; // -1.0 in Q16.16 format
        float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, -1.0f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (-1.0)\n");

        fixed_value = 32768; // 0.5 in Q16.16 format
        float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, 0.5f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (0.5)\n");

        fixed_value = -32768; // -0.5 in Q16.16 format
        float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, -0.5f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (-0.5)\n");

        fixed_value = 0; // 0.0 in Q16.16 format
        float_value = fixed_to_float(fixed_value);
        assert(compare_float(float_value, 0.0f, 0.0001f));
        printf("Test 14 passed: fixed_to_float (0.0)\n");
    }

    // Test 15: double_to_fixed
    {
        double value = 0.0;
        int64_t fixed_value = double_to_fixed(value);
        assert(fixed_value == 0); // 0.0 in Q32.32 format
        printf("Test 15 passed: double_to_fixed (0.0)\n");

        value = 1.0;
        fixed_value = double_to_fixed(value);
        assert(fixed_value == 4294967296); // 1.0 in Q32.32 format
        printf("Test 15 passed: double_to_fixed (1.0)\n");

        value = -1.0;
        fixed_value = double_to_fixed(value);
        assert(fixed_value == -4294967296); // -1.0 in Q32.32 format
        printf("Test 15 passed: double_to_fixed (-1.0)\n");

        value = 0.5;
        fixed_value = double_to_fixed(value);
        assert(fixed_value == 2147483648); // 0.5 in Q32.32 format
        printf("Test 15 passed: double_to_fixed (0.5)\n");

        value = -0.5;
        fixed_value = double_to_fixed(value);
        assert(fixed_value == -2147483648); // -0.5 in Q32.32 format
        printf("Test 15 passed: double_to_fixed (-0.5)\n");
    }

    // Test 16: float_to_fixed
    {
        float value = 1.0;
        int32_t fixed_value = float_to_fixed(value);
        assert(fixed_value == 65536); // 1.0 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (1.0)\n");

        value = -1.0;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == -65536); // -1.0 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (-1.0)\n");

        value = 0.5;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == 32768); // 0.5 in Q16.16 format

        printf("Test 16 passed: float_to_fixed (0.5)\n");

        value = -0.5;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == -32768); // -0.5 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (-0.5)\n");

        value = 0.0;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == 0); // 0.0 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (0.0)\n");

        value = 1.5;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == 98304); // 1.5 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (1.5)\n");

        value = -1.5;
        fixed_value = float_to_fixed(value);
        assert(fixed_value == -98304); // -1.5 in Q16.16 format
        printf("Test 16 passed: float_to_fixed (-1.5)\n");
    }

    // Test 17: fixed_to_double
    {
        int64_t fixed_value = 4294967296; // 1.0 in Q32.32 format
        double double_value = fixed_to_double(fixed_value);
        assert(compare_double(double_value, 1.0, 0.0001));
        printf("Test 17 passed: fixed_to_double (1.0)\n");

        fixed_value = -4294967296; // -1.0 in Q32.32 format
        double_value = fixed_to_double(fixed_value);
        assert(compare_double(double_value, -1.0, 0.0001));
        printf("Test 17 passed: fixed_to_double (-1.0)\n");

        fixed_value = 2147483648; // 0.5 in Q32.32 format
        double_value = fixed_to_double(fixed_value);
        assert(compare_double(double_value, 0.5, 0.0001));
        printf("Test 17 passed: fixed_to_double (0.5)\n");

        fixed_value = -2147483648; // -0.5 in Q32.32 format
        double_value = fixed_to_double(fixed_value);
        assert(compare_double(double_value, -0.5, 0.0001));
        printf("Test 17 passed: fixed_to_double (-0.5)\n");
    }
}

// Test function to test compare_float and compare_double
void
test_compare(void)
{
    // Test 1: Compare floats with exact same values
    {
        float a = 1.0f;
        float b = 1.0f;
        assert(compare_float(a, b, 0.0001f)); // Should pass, as the values are the same
        printf("Test 1 passed: compare_float (1.0f == 1.0f)\n");
    }

    // Test 2: Compare floats with values within tolerance
    {
        float a = 1.0001f;
        float b = 1.0002f;
        assert(compare_float(a, b, 0.0002f)); // Should pass, as the difference is within the tolerance
        printf("Test 2 passed: compare_float (1.0001f ~= 1.0002f within tolerance 0.0002f)\n");
    }

    // Test 3: Compare floats with values outside of tolerance
    {
        float a = 1.0f;
        float b = 1.1f;
        assert(!compare_float(a, b, 0.0001f)); // Should fail, as the difference exceeds the tolerance
        printf("Test 3 passed: compare_float (1.0f != 1.1f)\n");
    }

    // Test 4: Compare doubles with exact same values
    {
        double a = 1.0;
        double b = 1.0;
        assert(compare_double(a, b, 0.0001)); // Should pass, as the values are the same
        printf("Test 4 passed: compare_double (1.0 == 1.0)\n");
    }

    // Test 5: Compare doubles with values within tolerance
    {
        double a = 1.00001;
        double b = 1.00002;
        assert(compare_double(a, b, 0.00002)); // Should pass, as the difference is within the tolerance
        printf("Test 5 passed: compare_double (1.00001 ~= 1.00002 within tolerance 0.00002)\n");
    }

    // Test 6: Compare doubles with values outside of tolerance
    {
        double a = 1.0;
        double b = 1.2;
        assert(!compare_double(a, b, 0.0001)); // Should fail, as the difference exceeds the tolerance
        printf("Test 6 passed: compare_double (1.0 != 1.2)\n");
    }

    // Test 7: Basic double-to-fixed and back to double conversion
    {
        double value = 1.0;
        int64_t fixed_value = double_to_fixed(value);
        double double_value = fixed_to_double(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_double(double_value, 1.0, 0.0001));
        printf("Test 7 passed: double-to-fixed and back to double (1.0)\n");
    }
}

// Main function to run the tests
int
main(void)
{
    test_float_and_double();
    test_compare();
    return 0;
}
