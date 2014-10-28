#include "config.h"

#include <stdint.h>
#include <stdlib.h>

#include <emmintrin.h>

#ifdef ALIGN_EXTRA
#include "align/align_scan_128_16_debug.h"
#else
#include "align/align_scan_128_16.h"
#endif
#include "blosum/blosum_map.h"


#ifdef ALIGN_EXTRA
static inline void arr_store_si128(
        int *array,
        __m128i vH,
        int32_t t,
        int32_t seglen,
        int32_t d,
        int32_t dlen)
{
    array[(0*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 0);
    array[(1*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 1);
    array[(2*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 2);
    array[(3*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 3);
    array[(4*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 4);
    array[(5*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 5);
    array[(6*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 6);
    array[(7*seglen+t)*dlen + d] = (int16_t)_mm_extract_epi16(vH, 7);
}
#endif

#define PARALLEL_PREFIX_OP(vFt, gap, segLen)            \
{                                                       \
    union {                                             \
        __m128i m;                                      \
        int16_t v[8];                                   \
    } tmp;                                              \
    tmp.m = vFt;                                        \
    tmp.v[1] = MAX(tmp.v[0]-segLen*gap, tmp.v[1]);      \
    tmp.v[2] = MAX(tmp.v[1]-segLen*gap, tmp.v[2]);      \
    tmp.v[3] = MAX(tmp.v[2]-segLen*gap, tmp.v[3]);      \
    tmp.v[4] = MAX(tmp.v[3]-segLen*gap, tmp.v[4]);      \
    tmp.v[5] = MAX(tmp.v[4]-segLen*gap, tmp.v[5]);      \
    tmp.v[6] = MAX(tmp.v[5]-segLen*gap, tmp.v[6]);      \
    tmp.v[7] = MAX(tmp.v[6]-segLen*gap, tmp.v[7]);      \
    vFt = tmp.m;                                        \
}

#ifdef ALIGN_EXTRA
#define FNAME sw_scan_128_16_debug
#else
#define FNAME sw_scan_128_16
#endif

int FNAME(
        const char * const restrict s1, const int s1Len,
        const char * const restrict s2, const int s2Len,
        const int open, const int gap,
        const int8_t * const restrict matrix
#ifdef ALIGN_EXTRA
        , int * const restrict score_table
#endif
        )
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = 0;
    const int32_t n = 24; /* number of amino acids in table */
    const int32_t segWidth = 8; /* number of values in vector unit */
    int32_t segNum = 0;
    int32_t segLen = (s1Len + segWidth - 1) / segWidth;
    __m128i* pvP = (__m128i*)malloc(n * segLen * sizeof(__m128i));
    __m128i* pvE = (__m128i*)calloc(segLen, sizeof(__m128i));
    __m128i* pvHt = (__m128i*)calloc(segLen, sizeof(__m128i));
    __m128i* pvFt = (__m128i*)calloc(segLen, sizeof(__m128i));
    __m128i* pvH = (__m128i*)calloc(segLen, sizeof(__m128i));
    __m128i vGapO = _mm_set1_epi16(open);
    __m128i vGapE = _mm_set1_epi16(gap);
    __m128i vZero = _mm_setzero_si128();
    __m128i vOne = _mm_set1_epi16(1);
    int16_t score = NEG_INF_16;
    __m128i vMaxH = _mm_set1_epi16(NEG_INF_16);
    __m128i vQLimit = _mm_set1_epi16(s1Len);
    __m128i vQIndex_reset = _mm_set_epi16(
            7*segLen,
            6*segLen,
            5*segLen,
            4*segLen,
            3*segLen,
            2*segLen,
            1*segLen,
            0*segLen);

    /* Generate query profile.
     * Rearrange query sequence & calculate the weight of match/mismatch */
    {
        int16_t *t = (int16_t*)pvP;
        for (k=0; k<n; ++k) {
            for (i=0; i<segLen; ++i) {
                int32_t j = i;
                for (segNum=0; segNum<segWidth; ++segNum) {
                    *t++ = matrix[k*n + MAP_BLOSUM_[(unsigned char)s1[j]]];
                    j += segLen;
                }
            }
        }
    }

    /* initialize H and E */
    {
        int16_t *h = (int16_t*)pvH;
        int16_t *e = (int16_t*)pvE;
        for (i=0; i<segLen; ++i) {
            for (segNum=0; segNum<segWidth; ++segNum) {
                *h = 0;
                *e = NEG_INF_16;
                ++h;
                ++e;
            }
        }
    }

    /* outer loop over database sequence */
    for (j=0; j<s2Len; ++j) {
        __m128i vE;
        __m128i vHt;
        __m128i vFt;
        __m128i vH;
        __m128i *pvW;
        __m128i vW;
        __m128i vQIndex = vQIndex_reset;

        /* calculate E */
        for (i=0; i<segLen; ++i) {
            vH = _mm_load_si128(pvH+i);
            vE = _mm_load_si128(pvE+i);
            vE = _mm_max_epi16(
                    _mm_sub_epi16(vE, vGapE),
                    _mm_sub_epi16(vH, vGapO));
            _mm_store_si128(pvE+i, vE);
        }

        /* calculate Ht */
        vH = _mm_slli_si128(_mm_load_si128(pvH+(segLen-1)), 2);
        pvW = pvP + MAP_BLOSUM_[(unsigned char)s2[j]]*segLen;
        for (i=0; i<segLen; ++i) {
            vE = _mm_load_si128(pvE+i);
            vW = _mm_load_si128(pvW+i);
            vHt = _mm_max_epi16(
                    _mm_add_epi16(vH, vW),
                    vE);
            vH = _mm_load_si128(pvH+i);
            _mm_store_si128(pvHt+i, vHt);
        }

        /* calculate Ft */
        vHt = _mm_slli_si128(_mm_load_si128(pvHt+(segLen-1)), 2);
        vFt = _mm_set1_epi16(NEG_INF_16);
        for (i=0; i<segLen; ++i) {
            vFt = _mm_max_epi16(
                    _mm_sub_epi16(vFt, vGapE),
                    vHt);
            vHt = _mm_load_si128(pvHt+i);
        }
        PARALLEL_PREFIX_OP(vFt, gap, segLen)
        vHt = _mm_slli_si128(_mm_load_si128(pvHt+(segLen-1)), 2);
        vFt = _mm_slli_si128(vFt, 2);
        vFt = _mm_insert_epi16(vFt, NEG_INF_16, 0);
        for (i=0; i<segLen; ++i) {
            vFt = _mm_max_epi16(
                    _mm_sub_epi16(vFt, vGapE),
                    vHt);
            vHt = _mm_load_si128(pvHt+i);
            _mm_store_si128(pvFt+i, vFt);
        }

        /* calculate H */
        for (i=0; i<segLen; ++i) {
            vHt = _mm_load_si128(pvHt+i);
            vFt = _mm_load_si128(pvFt+i);
            vH = _mm_max_epi16(
                    vHt,
                    _mm_sub_epi16(vFt, vGapO));
            vH = _mm_max_epi16(vH, vZero);
            _mm_store_si128(pvH+i, vH);
#ifdef ALIGN_EXTRA
            arr_store_si128(score_table, vH, i, segLen, j, s2Len);
#endif
        }
        /* update max vector seen so far */
        {
            __m128i cond_max = _mm_cmpgt_epi16(vH,vMaxH);
            __m128i cond_lmt = _mm_cmplt_epi16(vQIndex,vQLimit);
            __m128i cond_all = _mm_and_si128(cond_max, cond_lmt);
            vMaxH = _mm_andnot_si128(cond_all, vMaxH);
            vMaxH = _mm_or_si128(vMaxH, _mm_and_si128(cond_all, vH));
            vQIndex = _mm_add_epi16(vQIndex, vOne);

        }
    }

    /* max in vec */
    for (j=0; j<segWidth; ++j) {
        int16_t value = (int16_t) _mm_extract_epi16(vMaxH, 7);
        if (value > score) {
            score = value;
        }
        vMaxH = _mm_slli_si128(vMaxH, 2);
    }

    free(pvP);
    free(pvE);
    free(pvHt);
    free(pvFt);
    free(pvH);

    return score;
}