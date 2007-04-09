/** 
 * @file SFMT-alti.h 
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

#ifndef SFMT_ALTI_H
#define SFMT_ALTI_H

union W128_T {
    vector unsigned int s;
    uint32_t u[4];
};

typedef union W128_T w128_t;

#ifdef __GNUC__
inline static vector unsigned int vec_recursion(vector unsigned int a,
						vector unsigned int b,
						vector unsigned int c,
						vector unsigned int d)
    __attribute__((always_inline));
#else
inline static vector unsigned int vec_recursion(vector unsigned int a,
						vector unsigned int b,
						vector unsigned int c,
						vector unsigned int d);
#endif

#endif
