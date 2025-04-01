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
