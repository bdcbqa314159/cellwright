#pragma once
#include <cstddef>

namespace magic {

// NaN-aware SIMD-accelerated aggregation over contiguous double arrays.
// Uses ARM NEON on Apple Silicon, AVX2 on x86_64, scalar fallback otherwise.

double simd_sum(const double* data, size_t count);
double simd_min(const double* data, size_t count);
double simd_max(const double* data, size_t count);
size_t simd_count_numeric(const double* data, size_t count);
double simd_sum_of_squares(const double* data, size_t count, double mean);

}  // namespace magic
