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
    // Test 1: Basic float-to-fixed and back to float conversion
    {
        uint32_t value = 0x3F800000; // 1.0 in IEEE 754 format
        int32_t fixed_value = float_to_fixed(value);
        float float_value = fixed_to_float(fixed_value);

        // Allow some small error in floating point comparison
        printf("float_value value: %f\n", float_value);
        assert(compare_float(float_value, 1.0f, 0.0001f));
        printf("Test 1 passed: float-to-fixed and back to float (1.0)\n");
    }

    // Test 2: Basic double-to-fixed and back to double conversion
    {
        double value = 1.0;
        int64_t fixed_value = double_to_fixed(value);
        double double_value = fixed_to_double(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_double(double_value, 1.0, 0.0001));
        printf("Test 2 passed: double-to-fixed and back to double (1.0)\n");
    }

    // Test 3: Convert negative float to fixed and back to float
    {
        uint32_t ieee_float = 0xBF800000; // -1.0 in IEEE 754 format
        int32_t fixed_value = float_to_fixed(ieee_float);
        float float_value = fixed_to_float(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_float(float_value, -1.0f, 0.0001f));
        printf("Test 3 passed: negative float-to-fixed and back to float (-1.0)\n");
    }

    // Test 4: Convert negative double to fixed and back to double
    {
        double value = -1.0;
        int64_t fixed_value = double_to_fixed(value);
        double double_value = fixed_to_double(fixed_value);

        // Allow some small error in floating point comparison
        assert(compare_double(double_value, -1.0, 0.0001));
        printf("Test 4 passed: negative double-to-fixed and back to double (-1.0)\n");
    }

    // Test 5: Complex math: Add two fixed-point values
    {
        uint32_t ieee_float1 = 0x3F800000; // 1.0
        uint32_t ieee_float2 = 0x40000000; // 2.0
        int32_t fixed1 = float_to_fixed(ieee_float1);
        int32_t fixed2 = float_to_fixed(ieee_float2);

        int32_t result_fixed = fixed1 + fixed2;
        float result_float = fixed_to_float(result_fixed);

        // Expected result: 1.0 + 2.0 = 3.0
        assert(compare_float(result_float, 3.0f, 0.0001f));
        printf("Test 5 passed: Complex math (1.0 + 2.0 = 3.0)\n");
    }

    // Test 6: Complex math: Subtract two fixed-point values
    {
        uint32_t ieee_float1 = 0x40000000; // 2.0
        uint32_t ieee_float2 = 0x3F800000; // 1.0
        int32_t fixed1 = float_to_fixed(ieee_float1);
        int32_t fixed2 = float_to_fixed(ieee_float2);

        int32_t result_fixed = fixed1 - fixed2;
        float result_float = fixed_to_float(result_fixed);

        // Expected result: 2.0 - 1.0 = 1.0
        assert(compare_float(result_float, 1.0f, 0.0001f));
        printf("Test 6 passed: Complex math (2.0 - 1.0 = 1.0)\n");
    }

    // Test 7: Complex math: Multiply two fixed-point values
    {
        uint32_t ieee_float1 = 0x3F800000; // 1.0
        uint32_t ieee_float2 = 0x40000000; // 2.0
        int32_t fixed1 = float_to_fixed(ieee_float1);
        int32_t fixed2 = float_to_fixed(ieee_float2);

        int64_t result_fixed = (int64_t)fixed1 * fixed2;
        result_fixed >>= 16; // Adjust result to fit Q16.16 format
        float result_float = fixed_to_float((int32_t)result_fixed);

        // Expected result: 1.0 * 2.0 = 2.0
        assert(compare_float(result_float, 2.0f, 0.0001f));
        printf("Test 7 passed: Complex math (1.0 * 2.0 = 2.0)\n");
    }

    // Test 8: Complex math: Divide two fixed-point values
    {
        uint32_t ieee_float1 = 0x40000000; // 2.0
        uint32_t ieee_float2 = 0x3F800000; // 1.0
        int32_t fixed1 = float_to_fixed(ieee_float1);
        int32_t fixed2 = float_to_fixed(ieee_float2);

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

        int64_t result_fixed = fixed1 + fixed2;
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

        int64_t result_fixed = fixed1 - fixed2;
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

        int64_t result_fixed = fixed1 * fixed2;
        result_fixed >>= 16; // Adjust result to fit Q16.16 format
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

        int64_t result_fixed = fixed1 << 16; // Scale up to avoid loss of precision
        result_fixed /= fixed2;
        double result_double = fixed_to_double(result_fixed);

        // Expected result: 2.0 / 1.0 = 2.0
        assert(compare_double(result_double, 2.0, 0.0001));
        printf("Test 12 passed: Complex math (2.0 / 1.0 = 2.0) [double]\n");
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
}

// Main function to run the tests
int
main(void)
{
    test_float_and_double();
    test_compare();
    return 0;
}
