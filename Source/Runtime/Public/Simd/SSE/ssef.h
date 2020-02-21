// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "Platform/Intrinsics.h"

BE_FORCE_INLINE ssef load_ps(const float *src) {
    return _mm_load_ps(src);
}

BE_FORCE_INLINE ssef loadu_ps(const float *src) {
    return _mm_loadu_ps(src);
}

BE_FORCE_INLINE ssef load1_ps(const float *src) {
    return _mm_load1_ps(src);
}

BE_FORCE_INLINE ssef loadnt_ps(const float *src) {
#if defined (__SSE4_1__)
    return _mm_castsi128_ps(_mm_stream_load_si128((__m128i *)src));
#else
    return _mm_load_ps(src);
#endif
}

BE_FORCE_INLINE void store_ps(const ssef &a, float *dst) {
    _mm_store_ps(dst, a);
}

BE_FORCE_INLINE void storeu_ps(const ssef &a, float *dst) {
    _mm_storeu_ps(dst, a);
}

BE_FORCE_INLINE void storent_ps(const ssef &a, float *dst) {
    _mm_stream_ps(dst, a);
}

BE_FORCE_INLINE ssef set_ps(float a, float b, float c, float d) {
    return _mm_set_ps(d, c, b, a);
}

BE_FORCE_INLINE ssef set1_ps(float a) {
    return _mm_set1_ps(a);
}

BE_FORCE_INLINE ssef setzero_ps() {
    return _mm_setzero_ps();
}

BE_FORCE_INLINE ssef epi32_to_ps(const __m128i a) {
    return _mm_cvtepi32_ps(a);
}

BE_FORCE_INLINE ssef abs_ps(const ssef &a) {
    return _mm_and_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff))); 
}

BE_FORCE_INLINE ssef sqr_ps(const ssef &a) {
    return _mm_mul_ps(a.m128, a.m128);
}

BE_FORCE_INLINE ssef sqrt_ps(const ssef &a) {
    return _mm_sqrt_ps(a.m128);
}

// Reciprocal with 12 bits of precision.
BE_FORCE_INLINE ssef rcp_ps(const ssef &a) {
    return _mm_rcp_ps(a.m128); 
}

// Reciprocal with at least 16 bits precision.
BE_FORCE_INLINE ssef rcp16_ps(const ssef &a) {
    ssef r = _mm_rcp_ps(a.m128);
    // Newton-Raphson approximation to improve precision.
    return _mm_mul_ps(r, _mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(a, r)));
}

// Reciprocal with close to full precision.
BE_FORCE_INLINE ssef rcp32_ps(const ssef &a) {
    ssef r = _mm_rcp_ps(a.m128);
    // Newton-Raphson approximation to improve precision.
    r = _mm_mul_ps(r, _mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(a, r)));
    return _mm_mul_ps(r, _mm_sub_ps(_mm_set1_ps(2.0f), _mm_mul_ps(a, r)));
}

// Divide with at least 16 bits precision.
BE_FORCE_INLINE ssef div16_ps(const ssef &a, const ssef &b) {
    return _mm_mul_ps(a.m128, rcp16_ps(b.m128));
}

// Divide with close to full precision.
BE_FORCE_INLINE ssef div32_ps(const ssef &a, const ssef &b) {
    return _mm_mul_ps(a.m128, rcp32_ps(b.m128));
}

// Reciprocal square root with 12 bits of precision.
BE_FORCE_INLINE ssef rsqrt_ps(const ssef &a) {
    return _mm_rsqrt_ps(a.m128); 
}

// Reciprocal square root with at least 16 bits precision.
BE_FORCE_INLINE ssef rsqrt16_ps(const ssef &a) {
    ssef r = _mm_rsqrt_ps(a.m128);
    // Newton-Raphson approximation to improve precision.
    return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.5f), r), _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(a, _mm_set1_ps(-0.5f)), r), _mm_mul_ps(r, r)));
}

// Reciprocal square root with close to full precision.
BE_FORCE_INLINE ssef rsqrt32_ps(const ssef &a) {
    ssef r = _mm_rsqrt_ps(a.m128);
    // Newton-Raphson approximation to improve precision.
    r = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.5f), r), _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(a, _mm_set1_ps(-0.5f)), r), _mm_mul_ps(r, r)));
    return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.5f), r), _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(a, _mm_set1_ps(-0.5f)), r), _mm_mul_ps(r, r)));
}

BE_FORCE_INLINE ssef operator+(const ssef &a) { return a; }
BE_FORCE_INLINE ssef operator-(const ssef &a) { return _mm_neg_ps(a.m128); }

BE_FORCE_INLINE ssef operator+(const ssef &a, const ssef &b) { return _mm_add_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef operator+(const ssef &a, const float &b) { return a + set1_ps(b); }
BE_FORCE_INLINE ssef operator+(const float &a, const ssef &b) { return set1_ps(a) + b; }

BE_FORCE_INLINE ssef operator-(const ssef &a, const ssef &b) { return _mm_sub_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef operator-(const ssef &a, const float &b) { return a - set1_ps(b); }
BE_FORCE_INLINE ssef operator-(const float &a, const ssef &b) { return set1_ps(a) - b; }

BE_FORCE_INLINE ssef operator*(const ssef &a, const ssef &b) { return _mm_mul_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef operator*(const ssef &a, const float &b) { return a * set1_ps(b); }
BE_FORCE_INLINE ssef operator*(const float &a, const ssef &b) { return set1_ps(a) * b; }

BE_FORCE_INLINE ssef operator/(const ssef &a, const ssef &b) { return a * rcp32_ps(b); }
BE_FORCE_INLINE ssef operator/(const ssef &a, const float &b) { return a * rcp32_ps(set1_ps(b)); }
BE_FORCE_INLINE ssef operator/(const float &a, const ssef &b) { return a * rcp32_ps(b); }

BE_FORCE_INLINE ssef operator^(const ssef &a, const ssef &b) { return _mm_xor_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef operator^(const ssef &a, const ssei &b) { return _mm_xor_ps(a.m128, _mm_castsi128_ps(b.m128i)); }

BE_FORCE_INLINE sseb operator==(const ssef &a, const ssef &b) { return _mm_cmpeq_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator==(const ssef &a, const float &b) { return a == set1_ps(b); }
BE_FORCE_INLINE sseb operator==(const float &a, const ssef &b) { return set1_ps(a) == b; }

BE_FORCE_INLINE sseb operator!=(const ssef &a, const ssef &b) { return _mm_cmpneq_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator!=(const ssef &a, const float &b) { return a != set1_ps(b); }
BE_FORCE_INLINE sseb operator!=(const float &a, const ssef &b) { return set1_ps(a) != b; }

BE_FORCE_INLINE sseb operator<(const ssef &a, const ssef &b) { return _mm_cmplt_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator<(const ssef &a, const float &b) { return a < set1_ps(b); }
BE_FORCE_INLINE sseb operator<(const float &a, const ssef &b) { return set1_ps(a) <  b; }

BE_FORCE_INLINE sseb operator>=(const ssef &a, const ssef &b) { return _mm_cmpnlt_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator>=(const ssef &a, const float &b) { return a >= set1_ps(b); }
BE_FORCE_INLINE sseb operator>=(const float &a, const ssef &b) { return set1_ps(a) >= b; }

BE_FORCE_INLINE sseb operator>(const ssef &a, const ssef &b) { return _mm_cmpnle_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator>(const ssef &a, const float &b) { return a > set1_ps(b); }
BE_FORCE_INLINE sseb operator>(const float &a, const ssef &b) { return set1_ps(a) > b; }

BE_FORCE_INLINE sseb operator<=(const ssef &a, const ssef &b) { return _mm_cmple_ps(a.m128, b.m128); }
BE_FORCE_INLINE sseb operator<=(const ssef &a, const float &b) { return a <= set1_ps(b); }
BE_FORCE_INLINE sseb operator<=(const float &a, const ssef &b) { return set1_ps(a) <= b; }

BE_FORCE_INLINE ssef &operator+=(ssef &a, const ssef &b) { return a = a + b; }
BE_FORCE_INLINE ssef &operator+=(ssef &a, const float &b) { return a = a + b; }

BE_FORCE_INLINE ssef &operator-=(ssef &a, const ssef &b) { return a = a - b; }
BE_FORCE_INLINE ssef &operator-=(ssef &a, const float &b) { return a = a - b; }

BE_FORCE_INLINE ssef &operator*=(ssef &a, const ssef &b) { return a = a * b; }
BE_FORCE_INLINE ssef &operator*=(ssef &a, const float &b) { return a = a * b; }

BE_FORCE_INLINE ssef &operator/=(ssef &a, const ssef &b) { return a = a / b; }
BE_FORCE_INLINE ssef &operator/=(ssef &a, const float &b) { return a = a / b; }

// dst = a * b + c
BE_FORCE_INLINE ssef madd_ps(const ssef &a, const ssef &b, const ssef &c) { return _mm_madd_ps(a.m128, b.m128, c.m128); }
// dst = -(a * b) + c
BE_FORCE_INLINE ssef nmadd_ps(const ssef &a, const ssef &b, const ssef &c) { return _mm_nmadd_ps(a.m128, b.m128, c.m128); }
// dst = a * b - c
BE_FORCE_INLINE ssef msub_ps(const ssef &a, const ssef &b, const ssef &c) { return _mm_msub_ps(a.m128, b.m128, c.m128); }
// dst = -(a * b) - c
BE_FORCE_INLINE ssef nmsub_ps(const ssef &a, const ssef &b, const ssef &c) { return _mm_nmsub_ps(a.m128, b.m128, c.m128); }

#ifdef __SSE4_1__
BE_FORCE_INLINE ssef floor_ps(const ssef &a) { return _mm_round_ps(a, _MM_FROUND_TO_NEG_INF); }
BE_FORCE_INLINE ssef ceil_ps(const ssef &a) { return _mm_round_ps(a, _MM_FROUND_TO_POS_INF); }
BE_FORCE_INLINE ssef trunc_ps(const ssef &a) { return _mm_round_ps(a, _MM_FROUND_TO_ZERO); }
BE_FORCE_INLINE ssef round_ps(const ssef &a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }
#else
BE_FORCE_INLINE ssef floor_ps(const ssef &a) { return ssef(floorf(a[0]), floorf(a[1]), floorf(a[2]), floorf(a[3])); }
BE_FORCE_INLINE ssef ceil_ps(const ssef &a) { return ssef(ceilf(a[0]), ceilf(a[1]), ceilf(a[2]), ceilf(a[3])); }
BE_FORCE_INLINE ssef trunc_ps(const ssef &a) { return ssef(truncf(a[0]), truncf(a[1]), truncf(a[2]), truncf(a[3])); }
BE_FORCE_INLINE ssef round_ps(const ssef &a) { return ssef(roundf(a[0]), roundf(a[1]), roundf(a[2]), roundf(a[3])); }
#endif

BE_FORCE_INLINE ssef frac_ps(const ssef &a) { return a - floor_ps(a); }

// Unpack to [a0, a1, b0, b1].
BE_FORCE_INLINE ssef unpacklo_ps(const ssef &a, const ssef &b) { return _mm_unpacklo_ps(a.m128, b.m128); }

// Unpack to [a2, a3, b2, b3].
BE_FORCE_INLINE ssef unpackhi_ps(const ssef &a, const ssef &b) { return _mm_unpackhi_ps(a.m128, b.m128); }

// Shuffles 4x32 bits floats using template parameters. x(0), y(1), z(2), w(3).
template <size_t i0, size_t i1, size_t i2, size_t i3>
BE_FORCE_INLINE ssef shuffle_ps(const ssef &a) { return _mm_perm_ps(a, _MM_SHUFFLE(i3, i2, i1, i0)); }

template <> 
BE_FORCE_INLINE ssef shuffle_ps<0, 0, 0, 0>(const ssef &a) { return _mm_splat_ps(a, 0); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<1, 1, 1, 1>(const ssef &a) { return _mm_splat_ps(a, 1); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<2, 2, 2, 2>(const ssef &a) { return _mm_splat_ps(a, 2); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<3, 3, 3, 3>(const ssef &a) { return _mm_splat_ps(a, 3); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<0, 1, 0, 1>(const ssef &a) { return _mm_movelh_ps(a, a); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<2, 3, 2, 3>(const ssef &a) { return _mm_movehl_ps(a, a); }

#ifdef __SSE3__
// _mm_moveldup_ps and _mm_movehdup_ps are better than shuffle, since they don't destroy the input operands (under non-AVX).
template <> 
BE_FORCE_INLINE ssef shuffle_ps<0, 0, 2, 2>(const ssef &a) { return _mm_movehdup_ps(a); }
template <> 
BE_FORCE_INLINE ssef shuffle_ps<1, 1, 3, 3>(const ssef &a) { return _mm_moveldup_ps(a); }
#endif

// Shuffles two 4x32 bits floats using template parameters. x(0), y(1), z(2), w(3).
template <size_t a0, size_t a1, size_t b0, size_t b1>
BE_FORCE_INLINE ssef shuffle_ps(const ssef &a, const ssef &b) { return _mm_shuffle_ps(a, b, _MM_SHUFFLE(b1, b0, a1, a0)); }

#if defined(__SSE4_1__) && !defined(__GNUC__)
// https://stackoverflow.com/questions/5526658/intel-sse-why-does-mm-extract-ps-return-int-instead-of-float
template <size_t i>
BE_FORCE_INLINE float extract_ps(const ssef &a) {
    union floatpun { int i; float f; } fp;
    fp.i = _mm_extract_ps(a, i);
    return fp.f;
}
#else
template <size_t i>
// NOTE: The _mm_cvtss_f32 intrinsic is free, it does not generate instructions, it only makes the compiler reinterpret the xmm register as a float.
BE_FORCE_INLINE float extract_ps(const ssef &a) { return _mm_cvtss_f32(shuffle_ps<i, i, i, i>(a)); }
#endif

template <>
BE_FORCE_INLINE float extract_ps<0>(const ssef &a) { return _mm_cvtss_f32(a); }

// Given a 4-channel single-precision ssef variable, returns the first channel 'x' as a float.
BE_FORCE_INLINE float x_ps(const ssef &a) { return extract_ps<0>(a); }

// Given a 4-channel single-precision ssef variable, returns the first channel 'y' as a float.
BE_FORCE_INLINE float y_ps(const ssef &a) { return extract_ps<1>(a); }

// Given a 4-channel single-precision ssef variable, returns the first channel 'z' as a float.
BE_FORCE_INLINE float z_ps(const ssef &a) { return extract_ps<2>(a); }

// Given a 4-channel single-precision ssef variable, returns the first channel 'w' as a float.
BE_FORCE_INLINE float w_ps(const ssef &a) { return extract_ps<3>(a); }

#ifdef __SSE4_1__
// Insert [32*src, 32*src+32] bits of b to a in [32*dst, 32*dst+32] bits with clear mask.
template <int dst, int src, int clearmask>
BE_FORCE_INLINE ssef insert_ps(const ssef &a, const ssef &b) { return _mm_insert_ps(a, b, (dst << 4) | (src << 6) | clearmask); }

// Insert [32*src, 32*src+32] bits of b to a in [32*dst, 32*dst+32] bits.
template <int dst, int src>
BE_FORCE_INLINE ssef insert_ps(const ssef &a, const ssef &b) { return insert<dst, src, 0>(a, b); }

// Insert b to a in [32*dst, 32*dst+32] bits.
template <int dst>
BE_FORCE_INLINE ssef insert_ps(const ssef &a, const float b) { return insert<dst, 0>(a, _mm_set_ss(b)); }
#else
// Insert [32*src, 32*src+32] bits of b to a in [32*dst, 32*dst+32] bits.
template <int dst, int src>
BE_FORCE_INLINE ssef insert_ps(const ssef &a, const ssef &b) { ssef c = a; c[dst & 3] = b[src & 3]; return c; }

// Insert b to a in [32*dst, 32*dst+32] bits.
template <int dst>
BE_FORCE_INLINE ssef insert_ps(const ssef &a, float b) { ssef c = a; c[dst & 3] = b; return c; }
#endif

// Select 4x32 bits floats using mask.
BE_FORCE_INLINE ssef select_ps(const ssef &a, const ssef &b, const sseb &mask) {
#if defined(__SSE4_1__)
    return _mm_blendv_ps(a, b, mask);
#else
    // dst = (a & !mask) | (b & mask)
    return _mm_or_ps(_mm_andnot_ps(_mm_castsi128_ps(mask), a), _mm_and_ps(_mm_castsi128_ps(mask), b));
#endif
}

BE_FORCE_INLINE ssef min_ps(const ssef &a, const ssef &b) { return _mm_min_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef min_ps(const ssef &a, const float &b) { return _mm_min_ps(a.m128, set1_ps(b)); }
BE_FORCE_INLINE ssef min_ps(const float &a, const ssef &b) { return _mm_min_ps(set1_ps(a), b.m128); }

BE_FORCE_INLINE ssef max_ps(const ssef &a, const ssef &b) { return _mm_max_ps(a.m128, b.m128); }
BE_FORCE_INLINE ssef max_ps(const ssef &a, const float &b) { return _mm_max_ps(a.m128, set1_ps(b)); }
BE_FORCE_INLINE ssef max_ps(const float &a, const ssef &b) { return _mm_max_ps(set1_ps(a), b.m128); }

// Broadcast minimum value of all 4 components.
BE_FORCE_INLINE ssef vreduce_min_ps(const ssef &v) {
    ssef h = min_ps(shuffle_ps<1, 0, 3, 2>(v), v);
    return min_ps(shuffle_ps<2, 3, 0, 1>(h), h);
}

// Broadcast maximum value of all 4 components.
BE_FORCE_INLINE ssef vreduce_max_ps(const ssef &v) {
    ssef h = max_ps(shuffle_ps<1, 0, 3, 2>(v), v);
    return max_ps(shuffle_ps<2, 3, 0, 1>(h), h);
}

BE_FORCE_INLINE float reduce_min_ps(const ssef &v) { return _mm_cvtss_f32(vreduce_min_ps(v)); }
BE_FORCE_INLINE float reduce_max_ps(const ssef &v) { return _mm_cvtss_f32(vreduce_max_ps(v)); }

// Return index of minimum component.
BE_FORCE_INLINE size_t select_min_ps(const ssef &v) { return __bsf(_mm_movemask_ps(v == vreduce_min_ps(v))); }

// Return index of maximum component.
BE_FORCE_INLINE size_t select_max_ps(const ssef &v) { return __bsf(_mm_movemask_ps(v == vreduce_max_ps(v))); }

// Return index of minimum component with valid index mask.
BE_FORCE_INLINE size_t select_min_ps(const sseb &validmask, const ssef &v) {
    const ssef a = select_ps(set1_ps(FLT_INFINITY), v, validmask);
    return __bsf(_mm_movemask_ps(validmask & (a == vreduce_min_ps(a))));
}

// Return index of maximum component with valid index mask.
BE_FORCE_INLINE size_t select_max_ps(const sseb &validmask, const ssef &v) {
    const ssef a = select_ps(set1_ps(-FLT_INFINITY), v, validmask);
    return __bsf(_mm_movemask_ps(validmask & (a == vreduce_max_ps(a))));
}

// Broadcast sums of all components.
BE_FORCE_INLINE ssef sum_ps(const ssef &a) {
    __m128 hadd = _mm_hadd_ps(a, a); // (x + y, z + w, x + y, z + w)
    return _mm_hadd_ps(hadd, hadd); // (x + y + z + w, x + y + z + w, x + y + z + w, x + y + z + w)
}

// Dot product.
BE_FORCE_INLINE float dot4_ps(const ssef &a, const ssef &b) {
    ssef mul = a * b;
    return x_ps(sum_ps(mul));
}

// Cross product.
BE_FORCE_INLINE ssef cross_ps(const ssef &a, const ssef &b) {
    ssef a_yzxw = shuffle_ps<1, 2, 0, 3>(a); // (a.y, a.z, a.x, a.w)
    ssef b_yzxw = shuffle_ps<1, 2, 0, 3>(b); // (b.y, b.z, b.x, b.w)
    ssef ab_yzxw = a_yzxw * b; // (a.y * b.x, a.z * b.y, a.x * b.z, a.w * b.w)

    return shuffle_ps<1, 2, 0, 3>(msub_ps(b_yzxw, a, ab_yzxw)); // (a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0)
}

// Transpose 4x4 matrix.
BE_FORCE_INLINE void mat4x4_transpose(const ssef &r0, const ssef &r1, const ssef &r2, const ssef &r3, ssef &c0, ssef &c1, ssef &c2, ssef &c3) {
    ssef l02 = unpacklo_ps(r0, r2); // m00, m20, m01, m21
    ssef h02 = unpackhi_ps(r0, r2); // m02, m22, m03, m23
    ssef l13 = unpacklo_ps(r1, r3); // m10, m30, m11, m31
    ssef h13 = unpackhi_ps(r1, r3); // m12, m32, m13, m33
    c0 = unpacklo_ps(l02, l13); // m00, m10, m20, m30
    c1 = unpackhi_ps(l02, l13); // m01, m11, m21, m31
    c2 = unpacklo_ps(h02, h13); // m02, m12, m22, m32
    c3 = unpackhi_ps(h02, h13); // m03, m13, m23, m33
}
