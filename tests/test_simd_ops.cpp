#include <gtest/gtest.h>
#include "core/SimdOps.hpp"
#include <cmath>
#include <limits>
#include <vector>

using namespace magic;

TEST(SimdOps, SumRegular) {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    EXPECT_DOUBLE_EQ(simd_sum(data.data(), data.size()), 15.0);
}

TEST(SimdOps, SumWithNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {1.0, nan, 3.0, nan, 5.0};
    EXPECT_DOUBLE_EQ(simd_sum(data.data(), data.size()), 9.0);
}

TEST(SimdOps, SumEmpty) {
    EXPECT_DOUBLE_EQ(simd_sum(nullptr, 0), 0.0);
}

TEST(SimdOps, SumLargeArray) {
    std::vector<double> data(10000, 1.0);
    EXPECT_DOUBLE_EQ(simd_sum(data.data(), data.size()), 10000.0);
}

TEST(SimdOps, MinRegular) {
    std::vector<double> data = {5.0, 2.0, 8.0, 1.0, 3.0};
    EXPECT_DOUBLE_EQ(simd_min(data.data(), data.size()), 1.0);
}

TEST(SimdOps, MinWithNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {nan, 5.0, nan, 2.0};
    EXPECT_DOUBLE_EQ(simd_min(data.data(), data.size()), 2.0);
}

TEST(SimdOps, MinEmpty) {
    double result = simd_min(nullptr, 0);
    EXPECT_EQ(result, std::numeric_limits<double>::infinity());
}

TEST(SimdOps, MaxRegular) {
    std::vector<double> data = {5.0, 2.0, 8.0, 1.0, 3.0};
    EXPECT_DOUBLE_EQ(simd_max(data.data(), data.size()), 8.0);
}

TEST(SimdOps, MaxWithNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {nan, 5.0, nan, 2.0};
    EXPECT_DOUBLE_EQ(simd_max(data.data(), data.size()), 5.0);
}

TEST(SimdOps, CountNumeric) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {1.0, nan, 3.0, nan, 5.0, 6.0};
    EXPECT_EQ(simd_count_numeric(data.data(), data.size()), 4u);
}

TEST(SimdOps, CountNumericAllNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {nan, nan, nan};
    EXPECT_EQ(simd_count_numeric(data.data(), data.size()), 0u);
}

TEST(SimdOps, SumOfSquares) {
    std::vector<double> data = {2.0, 4.0, 6.0};
    double mean = 4.0;
    // (2-4)^2 + (4-4)^2 + (6-4)^2 = 4 + 0 + 4 = 8
    EXPECT_DOUBLE_EQ(simd_sum_of_squares(data.data(), data.size(), mean), 8.0);
}

TEST(SimdOps, SumOfSquaresWithNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> data = {2.0, nan, 6.0};
    double mean = 4.0;
    // (2-4)^2 + (6-4)^2 = 4 + 4 = 8
    EXPECT_DOUBLE_EQ(simd_sum_of_squares(data.data(), data.size(), mean), 8.0);
}

TEST(SimdOps, LargeArrayWithNaN) {
    std::vector<double> data(10001);
    double nan = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = (i % 3 == 0) ? nan : static_cast<double>(i);
    }
    // Just verify no crash and result is finite
    double s = simd_sum(data.data(), data.size());
    EXPECT_FALSE(std::isnan(s));
    EXPECT_TRUE(std::isfinite(s));
}
