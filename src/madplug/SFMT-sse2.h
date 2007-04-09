/** 
 * @file SFMT-sse2.h 
 *
 * @brief SIMD oriented Fast Mersenne Twister(SFMT)
 * pseudorandom number generator
 *
 * @author Mutsuo Saito (Hiroshima University)
 * @author Makoto Matsumoto (Hiroshima University)
 *
 * Copyright (C) 2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima
 * University. All rights reserved.
 *
 * The new BSD License is applied to this software.
 * see LICENSE.txt
 */

#ifndef SFMT_SSE2_H
#define SFMT_SSE2_H
#include <emmintrin.h>

union W128_T {
    __m128i si;
    uint32_t u[4];
};

typedef union W128_T w128_t;

#ifdef __GNUC__
inline static __m128i mm_recursion(__m128i *a, __m128i *b, __m128i c,
				   __m128i d, __m128i mask)
    __attribute__((always_inline));
#else
inline static __m128i mm_recursion(__m128i *a, __m128i *b,
				   __m128i c, __m128i d, __m128i mask);
#endif
#endif
