#include "core/SimdOps.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__AVX2__)
#include <immintrin.h>
#endif

namespace magic {

// ── ARM NEON (Apple Silicon) — 2 doubles per op ─────────────────────────────

#if defined(__ARM_NEON)

double simd_sum(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    float64x2_t vsum = vdupq_n_f64(0.0);
    size_t i = 0;
    for (; i + 2 <= count; i += 2) {
        float64x2_t v = vld1q_f64(data + i);
        // NaN check: v == v is false for NaN
        uint64x2_t mask = vceqq_f64(v, v);
        float64x2_t clean = vbslq_f64(mask, v, vdupq_n_f64(0.0));
        vsum = vaddq_f64(vsum, clean);
    }
    double result = vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) result += data[i];
    }
    return result;
}

double simd_min(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double result = std::numeric_limits<double>::infinity();
    float64x2_t vmin = vdupq_n_f64(result);
    size_t i = 0;
    for (; i + 2 <= count; i += 2) {
        float64x2_t v = vld1q_f64(data + i);
        uint64x2_t mask = vceqq_f64(v, v);
        float64x2_t clean = vbslq_f64(mask, v, vdupq_n_f64(result));
        vmin = vminq_f64(vmin, clean);
    }
    result = std::min(vgetq_lane_f64(vmin, 0), vgetq_lane_f64(vmin, 1));
    for (; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] < result) result = data[i];
    }
    return result;
}

double simd_max(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double result = -std::numeric_limits<double>::infinity();
    float64x2_t vmax = vdupq_n_f64(result);
    size_t i = 0;
    for (; i + 2 <= count; i += 2) {
        float64x2_t v = vld1q_f64(data + i);
        uint64x2_t mask = vceqq_f64(v, v);
        float64x2_t clean = vbslq_f64(mask, v, vdupq_n_f64(result));
        vmax = vmaxq_f64(vmax, clean);
    }
    result = std::max(vgetq_lane_f64(vmax, 0), vgetq_lane_f64(vmax, 1));
    for (; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] > result) result = data[i];
    }
    return result;
}

size_t simd_count_numeric(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    uint64x2_t vcount = vdupq_n_u64(0);
    uint64x2_t one = vdupq_n_u64(1);
    size_t i = 0;
    for (; i + 2 <= count; i += 2) {
        float64x2_t v = vld1q_f64(data + i);
        uint64x2_t mask = vceqq_f64(v, v);
        vcount = vaddq_u64(vcount, vandq_u64(mask, one));
    }
    size_t result = vgetq_lane_u64(vcount, 0) + vgetq_lane_u64(vcount, 1);
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) ++result;
    }
    return result;
}

double simd_sum_of_squares(const double* data, size_t count, double mean) {
    assert(count == 0 || data != nullptr);
    float64x2_t vmean = vdupq_n_f64(mean);
    float64x2_t vsum = vdupq_n_f64(0.0);
    size_t i = 0;
    for (; i + 2 <= count; i += 2) {
        float64x2_t v = vld1q_f64(data + i);
        uint64x2_t mask = vceqq_f64(v, v);
        float64x2_t diff = vsubq_f64(v, vmean);
        float64x2_t sq = vmulq_f64(diff, diff);
        float64x2_t clean = vbslq_f64(mask, sq, vdupq_n_f64(0.0));
        vsum = vaddq_f64(vsum, clean);
    }
    double result = vgetq_lane_f64(vsum, 0) + vgetq_lane_f64(vsum, 1);
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) {
            double diff = data[i] - mean;
            result += diff * diff;
        }
    }
    return result;
}

// ── AVX2 (Intel) — 4 doubles per op ────────────────────────────────────────

#elif defined(__AVX2__)

#ifdef _MSC_VER
#include <intrin.h>
static inline int portable_popcountll(unsigned long long x) { return static_cast<int>(__popcnt64(x)); }
#else
static inline int portable_popcountll(unsigned long long x) { return __builtin_popcountll(x); }
#endif

double simd_sum(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        __m256d mask = _mm256_cmp_pd(v, v, _CMP_EQ_OQ);
        __m256d clean = _mm256_and_pd(v, mask);
        vsum = _mm256_add_pd(vsum, clean);
    }
    double buf[4];
    _mm256_storeu_pd(buf, vsum);
    double result = buf[0] + buf[1] + buf[2] + buf[3];
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) result += data[i];
    }
    return result;
}

double simd_min(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double inf = std::numeric_limits<double>::infinity();
    __m256d vmin = _mm256_set1_pd(inf);
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        __m256d mask = _mm256_cmp_pd(v, v, _CMP_EQ_OQ);
        __m256d clean = _mm256_blendv_pd(_mm256_set1_pd(inf), v, mask);
        vmin = _mm256_min_pd(vmin, clean);
    }
    double buf[4];
    _mm256_storeu_pd(buf, vmin);
    double result = std::min({buf[0], buf[1], buf[2], buf[3]});
    for (; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] < result) result = data[i];
    }
    return result;
}

double simd_max(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double ninf = -std::numeric_limits<double>::infinity();
    __m256d vmax = _mm256_set1_pd(ninf);
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        __m256d mask = _mm256_cmp_pd(v, v, _CMP_EQ_OQ);
        __m256d clean = _mm256_blendv_pd(_mm256_set1_pd(ninf), v, mask);
        vmax = _mm256_max_pd(vmax, clean);
    }
    double buf[4];
    _mm256_storeu_pd(buf, vmax);
    double result = std::max({buf[0], buf[1], buf[2], buf[3]});
    for (; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] > result) result = data[i];
    }
    return result;
}

size_t simd_count_numeric(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    size_t result = 0;
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        __m256d mask = _mm256_cmp_pd(v, v, _CMP_EQ_OQ);
        result += static_cast<size_t>(portable_popcountll(
            static_cast<unsigned long long>(_mm256_movemask_pd(mask))));
    }
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) ++result;
    }
    return result;
}

double simd_sum_of_squares(const double* data, size_t count, double mean) {
    assert(count == 0 || data != nullptr);
    __m256d vmean = _mm256_set1_pd(mean);
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        __m256d mask = _mm256_cmp_pd(v, v, _CMP_EQ_OQ);
        __m256d diff = _mm256_sub_pd(v, vmean);
        __m256d sq = _mm256_mul_pd(diff, diff);
        __m256d clean = _mm256_and_pd(sq, mask);
        vsum = _mm256_add_pd(vsum, clean);
    }
    double buf[4];
    _mm256_storeu_pd(buf, vsum);
    double result = buf[0] + buf[1] + buf[2] + buf[3];
    for (; i < count; ++i) {
        if (!std::isnan(data[i])) {
            double diff = data[i] - mean;
            result += diff * diff;
        }
    }
    return result;
}

// ── Scalar fallback ─────────────────────────────────────────────────────────

#else

double simd_sum(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double result = 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (!std::isnan(data[i])) result += data[i];
    }
    return result;
}

double simd_min(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double result = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] < result) result = data[i];
    }
    return result;
}

double simd_max(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    double result = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < count; ++i) {
        if (!std::isnan(data[i]) && data[i] > result) result = data[i];
    }
    return result;
}

size_t simd_count_numeric(const double* data, size_t count) {
    assert(count == 0 || data != nullptr);
    size_t result = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!std::isnan(data[i])) ++result;
    }
    return result;
}

double simd_sum_of_squares(const double* data, size_t count, double mean) {
    assert(count == 0 || data != nullptr);
    double result = 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (!std::isnan(data[i])) {
            double diff = data[i] - mean;
            result += diff * diff;
        }
    }
    return result;
}

#endif

}  // namespace magic
