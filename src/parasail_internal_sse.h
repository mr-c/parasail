/**
 * @file
 *
 * @author jeff.daily@pnnl.gov
 *
 * Copyright (c) 2014 Battelle Memorial Institute.
 *
 * All rights reserved. No warranty, explicit or implicit, provided.
 */
#ifndef _PARASAIL_INTERNAL_SSE_H_
#define _PARASAIL_INTERNAL_SSE_H_

#include <stdint.h>

#include <emmintrin.h>

typedef union __m128i_8 {
    __m128i m;
    int8_t v[16];
} __m128i_8_t;

typedef union __m128i_16 {
    __m128i m;
    int16_t v[8];
} __m128i_16_t;

typedef union __m128i_32 {
    __m128i m;
    int32_t v[4];
} __m128i_32_t;

__m128i * parasail_memalign_m128i(size_t alignment, size_t size);

#endif /* _PARASAIL_INTERNAL_SSE_H_ */
