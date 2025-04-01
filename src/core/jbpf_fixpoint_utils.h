// Copyright (c) Microsoft Corporation. All rights reserved.
/**
 * This contains the implementation of the fixed point helper functions.
 * These functions are used to convert between fixed point and double/uint types.
 * The fixed point representation is used to avoid using floating point operations
 * in the eBPF code, which is not supported in all environments.
 */

#include "jbpf_helper_utils.h"

/**
 * Converts a double to fixedpt, preserving accuracy.
 */
inline fixedpt
fixedpt_from_double(double value)
{
    return (fixedpt)(value * FIXEDPT_ONE);
}

/**
 * Converts a fixedpt to double, preserving accuracy.
 */
inline double
fixedpt_to_double(fixedpt value)
{
    return (double)value / FIXEDPT_ONE;
}

/**
 * Converts an unsigned integer to fixedpt, preserving accuracy.
 */
inline fixedpt
fixedpt_from_uint(unsigned int value)
{
    return (fixedpt)(value << FIXEDPT_FBITS);
}

/**
 * Converts a fixedpt to an unsigned integer, preserving accuracy.
 */
inline unsigned int
fixedpt_to_uint(fixedpt value)
{
    return (unsigned int)(value >> FIXEDPT_FBITS);
}

// Convert double to fixedpt (without using double in eBPF)
inline fixedpt
fixedpt_from_double_approx(int value, int scale_factor)
{
    return (fixedpt)(value * scale_factor);
}

// Convert fixedpt to int (approximation without double)
inline int
fixedpt_to_int_approx(fixedpt value, int scale_factor)
{
    return (int)(value / scale_factor);
}
