// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef JBPF_FIXPOINT_UTILS_H
#define JBPF_FIXPOINT_UTILS_H

#include "jbpf_helper_utils.h"
#include <stdint.h>
#include <math.h>

#define FIXED_POINT_SCALE (1 << 16) // Q16.16 scaling factor

/**
 * @brief Converts a fixed-point Q16.16 number to a floating-point number.
 * @param ieee_float The fixed-point number to convert.
 * @return The floating-point representation of the fixed-point number.
 */
static inline int32_t
float_to_fixed(uint32_t ieee_float)
{
    int sign = (ieee_float >> 31) & 1;                // Extract sign bit
    int exponent = ((ieee_float >> 23) & 0xFF) - 127; // Extract exponent (biased)
    uint32_t mantissa = (ieee_float & 0x7FFFFF);      // Get mantissa

    // Add the implicit leading 1 for normalized values
    if (exponent != -127) {
        mantissa |= (1 << 23); // Set the implicit leading 1 bit
    }

    // Adjust exponent to fit fixed-point Q16.16
    if (exponent >= 23) {
        // Shift left if exponent is large (integer part dominates)
        mantissa <<= (exponent - 23);
    } else {
        // Shift right if exponent is small (fractional part dominates)
        mantissa >>= (23 - exponent);
    }

    // Convert to fixed-point by multiplying with FIXED_POINT_SCALE
    int32_t fixed_value = (int32_t)(mantissa * FIXED_POINT_SCALE);

    // Apply the sign to the fixed-point value
    return sign ? -fixed_value : fixed_value;
}

/**
 * @brief Converts a fixed-point Q16.16 number to a floating-point number.
 * @param fixed The fixed-point number to convert.
 * @return The floating-point representation of the fixed-point number.
 */
static inline float
fixed_to_float(int32_t fixed)
{
    float float_value = (float)fixed / FIXED_POINT_SCALE;
    return float_value;
}

/**
 * @brief Converts a double-precision floating-point number to a fixed-point Q16.16 number.
 * @param value The double-precision floating-point number to convert.
 * @return The fixed-point representation of the double-precision floating-point number.
 */
static inline int64_t
double_to_fixed(double value)
{
    int sign = value < 0 ? -1 : 1;
    value = fabs(value);

    int exponent;
    double mantissa = frexp(value, &exponent);

    // Scale the mantissa to the fixed-point (Q32.32)
    int64_t fixed_value = (int64_t)(mantissa * FIXED_POINT_SCALE);

    // If exponent is positive, multiply by 2^exponent
    if (exponent > 0) {
        fixed_value *= (1 << exponent); // Multiply by 2^exponent for positive exponents
    }
    // If exponent is negative, divide by 2^(-exponent)
    else if (exponent < 0) {
        fixed_value /= (1 << -exponent); // Divide by 2^(-exponent) for negative exponents
    }

    // Apply the sign
    fixed_value *= sign;

    return fixed_value;
}

/**
 * @brief Converts a fixed-point Q32.32 number to a double-precision floating-point number.
 * @param fixed The fixed-point number to convert.
 * @return The double-precision floating-point representation of the fixed-point number.
 */
static inline double
fixed_to_double(int64_t fixed)
{
    double value = (double)fixed / FIXED_POINT_SCALE; // Correct division for Q32.32
    return value;
}

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

#endif // JBPF_FIXPOINT_UTILS_H