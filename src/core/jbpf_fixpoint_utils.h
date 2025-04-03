// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_FIXPOINT_UTILS_H
#define JBPF_FIXPOINT_UTILS_H

#include "jbpf_helper_utils.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define FIXED_POINT_SCALE (1 << 16)          // Q16.16 scaling factor
#define FIXED_POINT_SCALE_DOUBLE (1LL << 32) // Q32.32 scaling factor

/**
 * @brief Compares two floating-point numbers with a tolerance.
 * @param a The first floating-point number.
 * @param b The second floating-point number.
 * @param tolerance The tolerance for comparison.
 * @return 1 if the numbers are equal within the tolerance, 0 otherwise.
 */
static inline int
compare_float(float a, float b, float tolerance)
{
    return fabs(a - b) <= tolerance;
}

/**
 * @brief Compares two double-precision floating-point numbers with a tolerance.
 * @param a The first double-precision floating-point number.
 * @param b The second double-precision floating-point number.
 * @param tolerance The tolerance for comparison.
 * @return 1 if the numbers are equal within the tolerance, 0 otherwise.
 */
static inline int
compare_double(double a, double b, double tolerance)
{
    return fabs(a - b) <= tolerance;
}

/**
 * @brief Prints the details of a floating-point number in IEEE 754 format.
 * @param ieee_float The floating-point number to print.
 */
static inline void
print_float(float ieee_float)
{
    uint32_t value;
    memcpy(&value, &ieee_float, sizeof(float));
    printf("float: %f\n", ieee_float);
    printf("ieee_float: %x\n", value);
    printf("sign: %d\n", (value >> 31) & 1);
    printf("exponent: %d\n", ((value >> 23) & 0xFF) - 127);
    printf("mantissa: %x\n", value & 0x7FFFFF);
    printf("value: %d\n", value);
    printf("exponent (biased): %d\n", ((value >> 23) & 0xFF));
}

/**
 * @brief Prints the details of a double-precision floating-point number in IEEE 754 format.
 * @param ieee_double The double-precision floating-point number to print.
 */
static inline void
print_double(double ieee_double)
{
    uint64_t value;
    memcpy(&value, &ieee_double, sizeof(double));
    printf("double: %lf\n", ieee_double);
    printf("ieee_double: %lx\n", value);
    printf("sign: %ld\n", (value >> 63) & 1);
    printf("exponent: %ld\n", ((value >> 52) & 0x7FF) - 1023);
    printf("mantissa: %lx\n", value & 0xFFFFFFFFFFFFF);
    printf("value: %ld\n", value);
}

/**
 * @brief Prints the details of a fixed-point Q16.16 number.
 * @param fixed The fixed-point number to print.
 */
static inline void
print_fixed_float(int32_t fixed)
{
    printf("fixed: %d\n", fixed);
    printf("integer part: %d\n", fixed >> 16);
    printf("fractional part: %d\n", fixed & 0xFFFF);
    printf("fractional part (float): %f\n", (float)(fixed & 0xFFFF) / FIXED_POINT_SCALE);
    printf("float value: %f\n", (float)fixed / FIXED_POINT_SCALE);
}

/**
 * @brief Prints the details of a fixed-point Q32.32 number.
 * @param fixed The fixed-point number to print.
 */
static inline void
print_fixed_double(int64_t fixed)
{
    printf("fixed: %ld\n", fixed);
    printf("integer part: %ld\n", fixed >> 32);
    printf("fractional part: %ld\n", fixed & 0xFFFFFFFF);
    printf("fractional part (double): %lf\n", (double)(fixed & 0xFFFFFFFF) / FIXED_POINT_SCALE_DOUBLE);
    printf("double value: %lf\n", (double)fixed / FIXED_POINT_SCALE_DOUBLE);
}

/**
 * @brief Converts a floating-point IEEE 754 number (single precision) to a fixed-point Q16.16 number.
 * @param value The floating-point number to convert.
 * @return The fixed-point representation of the floating-point number.
 */
static inline int32_t
float_to_fixed(float value)
{
    uint32_t ieee_float;
    memcpy(&ieee_float, &value, sizeof(float));

    // Extract sign, exponent, and mantissa
    int sign = (ieee_float >> 31) & 1;
    int exponent = ((ieee_float >> 23) & 0xFF) - 127; // Unbiased exponent
    uint32_t mantissa = ieee_float & 0x7FFFFF;        // Extract mantissa (23 bits)

    // Handle normalized numbers (add implicit leading 1)
    if (exponent != -127) {
        mantissa |= (1 << 23);
    }

    // Convert to Q16.16 fixed-point format
    int32_t fixed_value;
    int shift = exponent - 23 + 16; // Adjust exponent for Q16.16 format

    if (shift > 0) {
        fixed_value = (int32_t)(mantissa << shift); // Shift left for large exponent
    } else {
        fixed_value = (int32_t)(mantissa >> -shift); // Shift right for small exponent
    }

    // Apply sign
    return sign ? -fixed_value : fixed_value;
}

/**
 * @brief Converts a fixed-point Q16.16 number to a floating-point IEEE 754 number (single precision).
 * @param fixed The fixed-point number to convert.
 * @return The floating-point representation of the fixed-point number.
 */
static inline float
fixed_to_float(uint32_t fixed)
{
    // Handle zero case directly
    if (fixed == 0) {
        return 0.0f;
    }

    // Extract the sign of the fixed-point value
    int sign = (fixed & 0x80000000) ? 1 : 0;         // sign = 0 for positive, 1 for negative
    uint32_t abs_value = sign == 0 ? fixed : -fixed; // Work with absolute value

    // The initial exponent assumes Q16.16 format
    int exponent = 127; // IEEE 754 bias (127) + 16 for the fixed-point fraction

    // Use the entire fixed-point value as the mantissa
    uint32_t mantissa = abs_value << 7; // Shift left to bring fractional bits into mantissa

    while (mantissa >= (1 << 24) && (exponent < 256)) { // Ensure mantissa fits in 24-bit range (1.xxxxx format)
        mantissa >>= 1;
        exponent++;
    }

    // printf("mantissa: %d, exponent: %d\n", mantissa, exponent);
    while (mantissa < (1 << 23) && exponent > 0) { // Ensure mantissa is properly normalized
        mantissa <<= 1;
        exponent--;
    }

    // Remove implicit leading 1 (IEEE 754 does not store it)
    mantissa &= 0x7FFFFF;

    // Construct the IEEE 754 float representation
    uint32_t ieee_float = (sign << 31) | ((exponent & 0xFF) << 23) | mantissa;

    // Convert bit representation to float
    float result;
    memcpy(&result, &ieee_float, sizeof(float));

    return result;
}

/**
 * @brief Converts a double-precision floating-point number to a fixed-point Q32.32 number.
 * @param value The double-precision floating-point number to convert.
 * @return The fixed-point representation of the double-precision floating-point number.
 */
static inline int64_t
double_to_fixed(double value)
{
    uint64_t ieee_double;
    memcpy(&ieee_double, &value, sizeof(double));

    // Extract sign, exponent, and mantissa
    int sign = (ieee_double >> 63) & 1;
    int exponent = ((ieee_double >> 52) & 0x7FF) - 1023; // Unbiased exponent (1023 is the bias for double)
    uint64_t mantissa = ieee_double & 0xFFFFFFFFFFFFF;   // Extract mantissa (52 bits)

    // Handle normalized numbers (add implicit leading 1)
    if (exponent != -1023) {
        mantissa |= (1ULL << 52); // Add implicit leading 1 for normalized numbers
    }

    // Convert to Q32.32 fixed-point format
    int shift =
        exponent - 52 + 32; // Adjust exponent for Q32.32 format (Q32.32 requires 32 bits for fractional precision)

    int64_t fixed_value;
    if (shift > 0) {
        fixed_value = (int64_t)(mantissa << shift); // Shift left for large exponent
    } else {
        fixed_value = (int64_t)(mantissa >> -shift); // Shift right for small exponent
    }

    // Apply sign to the fixed value
    return sign ? -fixed_value : fixed_value;
}

/**
 * @brief Converts a fixed-point Q32.32 number to a double-precision floating-point number.
 * @param fixed The fixed-point number to convert.
 * @return The double-precision floating-point representation of the fixed-point number.
 */
static inline double
fixed_to_double(uint64_t fixed)
{
    // Handle zero case directly
    if (fixed == 0) {
        return 0.0;
    }

    // Extract the sign of the fixed-point value
    int sign = (fixed & 0x8000000000000000) ? 1 : 0; // sign = 0 for positive, 1 for negative
    uint64_t abs_value = sign == 0 ? fixed : -fixed; // Work with absolute value

    // The initial exponent assumes Q32.32 format for double (32 bits for the integer part, 32 for the fractional part)
    int exponent = 1023; // IEEE 754 bias for double (1023) + 32 for the fixed-point fraction

    // Use the entire fixed-point value as the mantissa
    uint64_t mantissa = abs_value << 20;

    // **Normalization Step**
    while (mantissa >= (1ULL << 53) && (exponent < 2047)) { // Ensure mantissa fits in 53-bit range (1.xxxxx format)
        mantissa >>= 1;
        exponent++;
    }

    while (mantissa < (1ULL << 52) && exponent > 0) { // Ensure mantissa is properly normalized
        mantissa <<= 1;
        exponent--;
    }

    // Remove implicit leading 1 (IEEE 754 does not store it)
    mantissa &= 0xFFFFFFFFFFFFF;

    // Construct the IEEE 754 double representation
    uint64_t ieee_double = ((uint64_t)sign << 63) | ((uint64_t)(exponent & 0x7FF) << 52) | mantissa;

    // Convert bit representation to double
    double result;
    memcpy(&result, &ieee_double, sizeof(double));

    return result;
}

#endif // JBPF_FIXPOINT_UTILS_H
