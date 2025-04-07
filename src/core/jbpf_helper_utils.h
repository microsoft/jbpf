// Copyright (c) Microsoft Corporation. All rights reserved.
#ifndef _FIXEDPTC_H_
#define _FIXEDPTC_H_

/*
 * fixedptc.h is a 32-bit or 64-bit fixed point numeric library.
 *
 * The symbol FIXEDPT_BITS, if defined before this library header file
 * is included, determines the number of bits in the data type (its "width").
 * The default width is 32-bit (FIXEDPT_BITS=32) and it can be used
 * on any recent C99 compiler. The 64-bit precision (FIXEDPT_BITS=64) is
 * available on compilers which implement 128-bit "long long" types. This
 * precision has been tested on GCC 4.2+.
 *
 * The FIXEDPT_WBITS symbols governs how many bits are dedicated to the
 * "whole" part of the number (to the left of the decimal point). The larger
 * this width is, the larger the numbers which can be stored in the fixedpt
 * number. The rest of the bits (available in the FIXEDPT_FBITS symbol) are
 * dedicated to the fraction part of the number (to the right of the decimal
 * point).
 *
 * Since the number of bits in both cases is relatively low, many complex
 * functions (more complex than div & mul) take a large hit on the precision
 * of the end result because errors in precision accumulate.
 * This loss of precision can be lessened by increasing the number of
 * bits dedicated to the fraction part, but at the loss of range.
 *
 * Adventurous users might utilize this library to build two data types:
 * one which has the range, and one which has the precision, and carefully
 * convert between them (including adding two number of each type to produce
 * a simulated type with a larger range and precision).
 *
 * The ideas and algorithms have been cherry-picked from a large number
 * of previous implementations available on the Internet.
 * Tim Hartrick has contributed cleanup and 64-bit support patches.
 *
 * == Special notes for the 32-bit precision ==
 * Signed 32-bit fixed point numeric library for the 24.8 format.
 * The specific limits are -8388608.999... to 8388607.999... and the
 * most precise number is 0.00390625. In practice, you should not count
 * on working with numbers larger than a million or to the precision
 * of more than 2 decimal places. Make peace with the fact that PI
 * is 3.14 here. :)
 */

/*-
 * Copyright (c) 2010-2012 Ivan Voras <ivoras@freebsd.org>
 * Copyright (c) 2012 Tim Hartrick <tim@edgecast.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef FIXEDPT_BITS
#define FIXEDPT_BITS 32
#endif

#include <stdint.h>
#include <string.h>

#if FIXEDPT_BITS == 32
typedef int32_t fixedpt;
typedef int64_t fixedptd;
typedef uint32_t fixedptu;
typedef uint64_t fixedptud;
#elif FIXEDPT_BITS == 64
typedef int64_t fixedpt;
typedef __int128_t fixedptd;
typedef uint64_t fixedptu;
typedef __uint128_t fixedptud;
#else
#error "FIXEDPT_BITS must be equal to 32 or 64"
#endif

#ifndef FIXEDPT_WBITS
#define FIXEDPT_WBITS 24
#endif

#if FIXEDPT_WBITS >= FIXEDPT_BITS
#error "FIXEDPT_WBITS must be less than or equal to FIXEDPT_BITS"
#endif

#define FIXEDPT_VCSID "$Id$"

#define FIXEDPT_FBITS (FIXEDPT_BITS - FIXEDPT_WBITS)
#define FIXEDPT_FMASK (((fixedpt)1 << FIXEDPT_FBITS) - 1)
// Smallest difference between fixed-point numbers with FIXEDPT_BITS fractional bits
#define FIXEDPT_EPSILON ((fixedpt)100)

#define fixedpt_rconst(R) ((fixedpt)((R)*FIXEDPT_ONE + ((R) >= 0 ? 0.5 : -0.5)))
#define fixedpt_fromint(I) ((fixedptd)(I) << FIXEDPT_FBITS)
#define fixedpt_toint(F) ((F) >> FIXEDPT_FBITS)
#define fixedpt_add(A, B) ((A) + (B))
#define fixedpt_sub(A, B) ((A) - (B))
#define fixedpt_xmul(A, B) ((fixedpt)(((fixedptd)(A) * (fixedptd)(B)) >> FIXEDPT_FBITS))
#define fixedpt_xdiv(A, B) ((fixedpt)(((fixedptd)(A) << FIXEDPT_FBITS) / (fixedptd)(B)))
#define fixedpt_fracpart(A) ((fixedpt)(A)&FIXEDPT_FMASK)

#define FIXEDPT_ZERO ((fixedpt)0)
#define FIXEDPT_ONE ((fixedpt)((fixedpt)1 << FIXEDPT_FBITS))
#define FIXEDPT_ONE_HALF (FIXEDPT_ONE >> 1)
#define FIXEDPT_TWO (FIXEDPT_ONE + FIXEDPT_ONE)
#define FIXEDPT_PI fixedpt_rconst(3.14159265358979323846)
#define FIXEDPT_TWO_PI fixedpt_rconst(2 * 3.14159265358979323846)
#define FIXEDPT_HALF_PI fixedpt_rconst(3.14159265358979323846 / 2)
#define FIXEDPT_E fixedpt_rconst(2.7182818284590452354)

#define fixedpt_abs(A) ((A) < 0 ? -(A) : (A))

/**
 * @brief fixedpt is meant to be usable in environments without floating point support
 * (e.g. microcontrollers, kernels), so we can't use floating point types directly.
 * Putting them only in macros will effectively make them optional.
 * @param T The fixedpt number to convert
 * @return The floating point representation of the fixedpt number
 * @ingroup core
 */
#define fixedpt_tofloat(T) ((float)((T) * ((float)(1) / (float)(1L << FIXEDPT_FBITS))))

/**
 * @brief Multiplies two fixedpt numbers, returns the result.
 * @param A The first fixedpt number
 * @param B The second fixedpt number
 * @return The result of the multiplication
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_mul(fixedpt A, fixedpt B)
{
    return (((fixedptd)A * (fixedptd)B) >> FIXEDPT_FBITS);
}

/**
 * @brief Divides two fixedpt numbers, returns the result.
 * @param A The first fixedpt number
 * @param B The second fixedpt number
 * @return The result of the division
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_div(fixedpt A, fixedpt B)
{
    return (((fixedptd)A << FIXEDPT_FBITS) / (fixedptd)B);
}

/*
 * Note: adding and substracting fixedpt numbers can be done by using
 * the regular integer operators + and -.
 */

/**
 * @brief Convert the given fixedpt number to a decimal string.
 * @param A The fixedpt number to convert
 * @param str The string buffer to write the result to
 * @param max_dec argument specifies how many decimal digits to the right
 * of the decimal point to generate. If set to -1, the "default" number
 * of decimal digits will be used (2 for 32-bit fixedpt width, 10 for
 * 64-bit fixedpt width); If set to -2, "all" of the digits will
 * be returned, meaning there will be invalid, bogus digits outside the
 * specified precisions.
 * @return the decimal string
 * @ingroup core
 */
static __always_inline void
fixedpt_str(fixedpt A, char* str, int max_dec)
{
    int ndec = 0, slen = 0;
    char tmp[12] = {0};
    fixedptud fr, ip;
    const fixedptud one = (fixedptud)1 << FIXEDPT_BITS;
    const fixedptud mask = one - 1;

    if (max_dec == -1)
#if FIXEDPT_BITS == 32
#if FIXEDPT_WBITS > 16
        max_dec = 2;
#else
        max_dec = 4;
#endif
#elif FIXEDPT_BITS == 64
        max_dec = 10;
#else
#error Invalid width
#endif
    else if (max_dec == -2)
        max_dec = 15;

    if (A < 0) {
        str[slen++] = '-';
        A *= -1;
    }

    ip = fixedpt_toint(A);
    do {
        tmp[ndec++] = '0' + ip % 10;
        ip /= 10;
    } while (ip != 0);

    while (ndec > 0)
        str[slen++] = tmp[--ndec];
    str[slen++] = '.';

    fr = (fixedpt_fracpart(A) << FIXEDPT_WBITS) & mask;
    do {
        fr = (fr & mask) * 10;

        str[slen++] = '0' + (fr >> FIXEDPT_BITS) % 10;
        ndec++;
    } while (fr != 0 && ndec < max_dec);

    if (ndec > 1 && str[slen - 1] == '0')
        str[slen - 1] = '\0'; /* cut off trailing 0 */
    else
        str[slen] = '\0';
}

/**
 * @brief Converts the given fixedpt number into a string, using a static (non-threadsafe) string buffer
 * @param A The fixedpt number to convert
 * @param max_dec The maximum number of decimal digits to generate
 * @return The string representation of the fixedpt number
 * @ingroup core
 */
static __always_inline char*
fixedpt_cstr(const fixedpt A, const int max_dec)
{
    static char str[25];

    fixedpt_str(A, str, max_dec);
    return (str);
}

/**
 * @brief Returns the square root of the given number, or -1 in case of error
 * @param A The fixedpt number to convert
 * @return The square root of the given number, or -1 in case of error
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_sqrt(fixedpt A)
{
    int invert = 0;
    int iter = FIXEDPT_FBITS;
    int l, i;

    if (A < 0)
        return (-1);
    if (A == 0 || A == FIXEDPT_ONE)
        return (A);
    if (A < FIXEDPT_ONE && A > 6) {
        invert = 1;
        A = fixedpt_div(FIXEDPT_ONE, A);
    }
    if (A > FIXEDPT_ONE) {
        int s = A;

        iter = 0;
        while (s > 0) {
            s >>= 2;
            iter++;
        }
    }

    /* Newton's iterations */
    l = (A >> 1) + 1;
    for (i = 0; i < iter; i++)
        l = (l + fixedpt_div(A, l)) >> 1;
    if (invert)
        return (fixedpt_div(FIXEDPT_ONE, l));
    return (l);
}

/**
 * @brief Returns the sine of the given fixedpt number
 * @param fp The fixedpt number
 * @return The sine of the given fixedpt number
 * @note The loss of precision is extraordinary!
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_sin(fixedpt fp)
{
    int sign = 1;
    fixedpt sqr, result;
    const fixedpt SK[2] = {fixedpt_rconst(7.61e-03), fixedpt_rconst(1.6605e-01)};

    fp %= 2 * FIXEDPT_PI;
    if (fp < 0)
        fp = FIXEDPT_PI * 2 + fp;
    if ((fp > FIXEDPT_HALF_PI) && (fp <= FIXEDPT_PI))
        fp = FIXEDPT_PI - fp;
    else if ((fp > FIXEDPT_PI) && (fp <= (FIXEDPT_PI + FIXEDPT_HALF_PI))) {
        fp = fp - FIXEDPT_PI;
        sign = -1;
    } else if (fp > (FIXEDPT_PI + FIXEDPT_HALF_PI)) {
        fp = (FIXEDPT_PI << 1) - fp;
        sign = -1;
    }
    sqr = fixedpt_mul(fp, fp);
    result = SK[0];
    result = fixedpt_mul(result, sqr);
    result -= SK[1];
    result = fixedpt_mul(result, sqr);
    result += FIXEDPT_ONE;
    result = fixedpt_mul(result, fp);
    return sign * result;
}

/**
 * @brief Returns the cosine of the given fixedpt number
 * @param A The fixedpt number
 * @return The cosine of the given fixedpt number
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_cos(fixedpt A)
{
    return (fixedpt_sin(FIXEDPT_HALF_PI - A));
}

/**
 * @brief Returns the tangens of the given fixedpt number
 * @param A The fixedpt number
 * @return The tangens of the given fixedpt number
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_tan(fixedpt A)
{
    return fixedpt_div(fixedpt_sin(A), fixedpt_cos(A));
}

/**
 * @brief Returns the value exp(x), i.e. e^x of the given fixedpt number
 * @param fp The fixedpt number
 * @return The value exp(x), i.e. e^x of the given fixedpt number
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_exp(fixedpt fp)
{
    fixedpt xabs, k, z, R, xp;
    const fixedpt LN2 = fixedpt_rconst(0.69314718055994530942);
    const fixedpt LN2_INV = fixedpt_rconst(1.4426950408889634074);
    const fixedpt EXP_P[5] = {
        fixedpt_rconst(1.66666666666666019037e-01),
        fixedpt_rconst(-2.77777777770155933842e-03),
        fixedpt_rconst(6.61375632143793436117e-05),
        fixedpt_rconst(-1.65339022054652515390e-06),
        fixedpt_rconst(4.13813679705723846039e-08),
    };

    if (fp == 0)
        return (FIXEDPT_ONE);
    xabs = fixedpt_abs(fp);
    k = fixedpt_mul(xabs, LN2_INV);
    k += FIXEDPT_ONE_HALF;
    k &= ~FIXEDPT_FMASK;
    if (fp < 0)
        k = -k;
    fp -= fixedpt_mul(k, LN2);
    z = fixedpt_mul(fp, fp);
    /* Taylor */
    R = FIXEDPT_TWO +
        fixedpt_mul(
            z,
            EXP_P[0] +
                fixedpt_mul(
                    z, EXP_P[1] + fixedpt_mul(z, EXP_P[2] + fixedpt_mul(z, EXP_P[3] + fixedpt_mul(z, EXP_P[4])))));
    xp = FIXEDPT_ONE + fixedpt_div(fixedpt_mul(fp, FIXEDPT_TWO), R - fp);
    if (k < 0)
        k = FIXEDPT_ONE >> (-k >> FIXEDPT_FBITS);
    else
        k = FIXEDPT_ONE << (k >> FIXEDPT_FBITS);
    return (fixedpt_mul(k, xp));
}

/**
 * @brief Returns the natural logarithm of the given fixedpt number
 * @param x The fixedpt number
 * @return The natural logarithm of the given fixedpt number
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_ln(fixedpt x)
{
    fixedpt log2, xi;
    fixedpt f, s, z, w, R;
    const fixedpt LN2 = fixedpt_rconst(0.69314718055994530942);
    const fixedpt LG[7] = {
        fixedpt_rconst(6.666666666666735130e-01),
        fixedpt_rconst(3.999999999940941908e-01),
        fixedpt_rconst(2.857142874366239149e-01),
        fixedpt_rconst(2.222219843214978396e-01),
        fixedpt_rconst(1.818357216161805012e-01),
        fixedpt_rconst(1.531383769920937332e-01),
        fixedpt_rconst(1.479819860511658591e-01)};

    if (x < 0)
        return (0);
    if (x == 0)
        return 0xffffffff;

    log2 = 0;
    xi = x;
    while (xi > FIXEDPT_TWO) {
        xi >>= 1;
        log2++;
    }
    f = xi - FIXEDPT_ONE;
    s = fixedpt_div(f, FIXEDPT_TWO + f);
    z = fixedpt_mul(s, s);
    w = fixedpt_mul(z, z);
    R = fixedpt_mul(w, LG[1] + fixedpt_mul(w, LG[3] + fixedpt_mul(w, LG[5]))) +
        fixedpt_mul(z, LG[0] + fixedpt_mul(w, LG[2] + fixedpt_mul(w, LG[4] + fixedpt_mul(w, LG[6]))));
    return (fixedpt_mul(LN2, (log2 << FIXEDPT_FBITS)) + f - fixedpt_mul(s, f - R));
}

/**
 * @brief Returns the logarithm of the given base of the given fixedpt number
 * @param x The fixedpt number
 * @param base The base
 * @return The logarithm of the given base of the given fixedpt number
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_log(fixedpt x, fixedpt base)
{
    return (fixedpt_div(fixedpt_ln(x), fixedpt_ln(base)));
}

/**
 * @brief Returns the power value (n^exp) of the given fixedpt numbers
 * @param n The base
 * @param exp The exponent
 * @return The power value (n^exp) of the given fixedpt numbers
 * @ingroup core
 */
static __always_inline fixedpt
fixedpt_pow(fixedpt n, fixedpt exp)
{
    if (exp == 0)
        return (FIXEDPT_ONE);
    if (n < 0)
        return 0;
    return (fixedpt_exp(fixedpt_mul(fixedpt_ln(n), exp)));
}

/**
 * @brief Converts a fixed-point number to a double-precision floating-point number.
 * @param fixed The fixed-point number to convert.
 * @return The double-precision floating-point representation of the fixed-point number.
 */
static __always_inline double
fixed_to_double(fixedpt fixed)
{
    // Handle zero case directly
    if (fixed == FIXEDPT_ZERO) {
        return 0.0;
    }

    // Extract the sign of the fixed-point value
    int sign = (fixed < 0) ? 1 : 0;
    uint64_t abs_value = sign == 0 ? fixed : -fixed; // Work with absolute value

    int exponent = 1023;

    // Use the entire fixed-point value as the mantissa
    uint64_t mantissa = (uint64_t)abs_value;
#if FIXEDPT_FBITS <= 52
    mantissa <<= (52 - FIXEDPT_FBITS); // Shift left to bring fractional bits into mantissa
#else
    mantissa >>= (FIXEDPT_FBITS - 52); // Shift right to fit into mantissa
#endif

    // Normalize the mantissa and adjust the exponent accordingly
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

/**
 * @brief Converts a fixed-point number to a floating-point IEEE 754 number (single precision).
 * @param fixed The fixed-point number to convert.
 * @return The floating-point representation of the fixed-point number.
 */
static __always_inline float
fixed_to_float(fixedpt fixed)
{
    // Handle zero case directly
    if (fixed == FIXEDPT_ZERO) {
        return 0.0f;
    }

    // Extract the sign of the fixed-point value
    int sign = (fixed < 0) ? 1 : 0;
    uint32_t abs_value = sign == 0 ? fixed : -fixed; // Work with absolute value

    int exponent = 127;
    uint32_t mantissa = abs_value;

// Use the entire fixed-point value as the mantissa
#if FIXEDPT_FBITS <= 23
    mantissa <<= (23 - FIXEDPT_FBITS); // Shift left to bring fractional bits into mantissa
#else
    mantissa >>= (FIXEDPT_FBITS - 23); // Shift right to fit into mantissa
#endif

    while (mantissa >= (1 << 24) && (exponent < 256)) { // Ensure mantissa fits in 24-bit range (1.xxxxx format)
        mantissa >>= 1;
        exponent++;
    }

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
 * @brief Converts a floating-point IEEE 754 number (single precision) to a fixed-point Q16.16 number.
 * @param value The floating-point number to convert.
 * @return The fixed-point representation of the floating-point number.
 */
static __always_inline fixedpt
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

    int32_t fixed_value;
    int shift = exponent - 23 + FIXEDPT_FBITS;

    if (shift > 0) {
        fixed_value = (int32_t)(mantissa << shift); // Shift left for large exponent
    } else {
        fixed_value = (int32_t)(mantissa >> -shift); // Shift right for small exponent
    }

    // Apply sign
    return sign ? -fixed_value : fixed_value;
}

/**
 * @brief Converts a double-precision floating-point number to a fixed-point Q32.32 number.
 * @param value The double-precision floating-point number to convert.
 * @return The fixed-point representation of the double-precision floating-point number.
 */
static __always_inline fixedpt
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
    int shift = exponent - 52 + FIXEDPT_FBITS;

    int64_t fixed_value;
    if (shift > 0) {
        fixed_value = (int64_t)(mantissa << shift); // Shift left for large exponent
    } else {
        fixed_value = (int64_t)(mantissa >> -shift); // Shift right for small exponent
    }

    // Apply sign to the fixed value
    return sign ? -fixed_value : fixed_value;
}

#endif
