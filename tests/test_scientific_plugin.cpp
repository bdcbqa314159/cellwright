#include <gtest/gtest.h>
#include "../plugins/scientific/scientific.hpp"
#include "core/CellValue.hpp"
#include <cmath>

using namespace magic;

class ScientificTest : public ::testing::Test {
protected:
    magic::plugins::Scientific plugin;
};

// ── sigmoid ────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, SigmoidZero) {
    auto r = plugin.call("sigmoid", {CellValue{0.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 0.5);
}

TEST_F(ScientificTest, SigmoidLargePositive) {
    auto r = plugin.call("sigmoid", {CellValue{100.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 1.0, 1e-10);
}

TEST_F(ScientificTest, SigmoidLargeNegative) {
    auto r = plugin.call("sigmoid", {CellValue{-100.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 0.0, 1e-10);
}

// ── logit ──────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, LogitHalf) {
    auto r = plugin.call("logit", {CellValue{0.5}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 0.0, 1e-12);
}

TEST_F(ScientificTest, LogitZero) {
    auto r = plugin.call("logit", {CellValue{0.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

TEST_F(ScientificTest, LogitOne) {
    auto r = plugin.call("logit", {CellValue{1.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── entropy ────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, EntropyFairCoin) {
    auto r = plugin.call("entropy", {CellValue{0.5}, CellValue{0.5}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), std::log(2.0), 1e-12);
}

TEST_F(ScientificTest, EntropyCertain) {
    auto r = plugin.call("entropy", {CellValue{1.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 0.0);
}

TEST_F(ScientificTest, EntropyInvalidProb) {
    auto r = plugin.call("entropy", {CellValue{0.5}, CellValue{-0.1}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── decay ──────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, DecayBasic) {
    auto r = plugin.call("decay", {CellValue{100.0}, CellValue{0.1}, CellValue{10.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 100.0 * std::exp(-1.0), 1e-6);
}

// ── halflife ───────────────────────────────────────────────────────────────

TEST_F(ScientificTest, HalflifeTwoPeriods) {
    // 100 * (0.5)^(10/5) = 100 * 0.25 = 25
    auto r = plugin.call("halflife", {CellValue{100.0}, CellValue{5.0}, CellValue{10.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 25.0);
}

TEST_F(ScientificTest, HalflifeZeroDivision) {
    auto r = plugin.call("halflife", {CellValue{100.0}, CellValue{0.0}, CellValue{10.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::DIV0);
}

// ── blackbody ──────────────────────────────────────────────────────────────

TEST_F(ScientificTest, BlackbodyPositive) {
    // Sun-like: 500 nm, 5778 K → should be a large positive number
    auto r = plugin.call("blackbody", {CellValue{500.0}, CellValue{5778.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_GT(as_number(r), 0.0);
    EXPECT_GT(as_number(r), 1e12);  // spectral radiance is large
}

TEST_F(ScientificTest, BlackbodyInvalidWavelength) {
    auto r = plugin.call("blackbody", {CellValue{0.0}, CellValue{5778.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── doppler ────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, DopplerNoShift) {
    auto r = plugin.call("doppler", {CellValue{1000.0}, CellValue{0.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 1000.0);
}

TEST_F(ScientificTest, DopplerReceding) {
    // Receding source → lower observed frequency
    auto r = plugin.call("doppler", {CellValue{1000.0}, CellValue{1000.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_LT(as_number(r), 1000.0);
}

// ── ideal_gas ──────────────────────────────────────────────────────────────

TEST_F(ScientificTest, IdealGasSolveT) {
    // PV = nRT → T = PV/(nR)
    // P=101325, V=0.0224, n=1 → T = 101325*0.0224/(1*8.314462618) ≈ 272.96
    auto r = plugin.call("ideal_gas", {
        CellValue{101325.0}, CellValue{0.0224}, CellValue{1.0},
        CellValue{std::string("T")}
    });
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 101325.0 * 0.0224 / 8.314462618, 0.01);
}

TEST_F(ScientificTest, IdealGasInvalidTarget) {
    auto r = plugin.call("ideal_gas", {
        CellValue{1.0}, CellValue{1.0}, CellValue{1.0},
        CellValue{std::string("X")}
    });
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

TEST_F(ScientificTest, IdealGasNotString) {
    auto r = plugin.call("ideal_gas", {
        CellValue{1.0}, CellValue{1.0}, CellValue{1.0}, CellValue{1.0}
    });
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── normalize ──────────────────────────────────────────────────────────────

TEST_F(ScientificTest, NormalizeMidpoint) {
    auto r = plugin.call("normalize", {CellValue{5.0}, CellValue{0.0}, CellValue{10.0}});
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 0.5);
}

TEST_F(ScientificTest, NormalizeDivByZero) {
    auto r = plugin.call("normalize", {CellValue{5.0}, CellValue{5.0}, CellValue{5.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::DIV0);
}

// ── pearson ────────────────────────────────────────────────────────────────

TEST_F(ScientificTest, PearsonPerfectCorrelation) {
    // pairs: (1,1),(2,2),(3,3) → r = 1.0
    auto r = plugin.call("pearson", {
        CellValue{1.0}, CellValue{1.0},
        CellValue{2.0}, CellValue{2.0},
        CellValue{3.0}, CellValue{3.0},
    });
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 1.0, 1e-12);
}

TEST_F(ScientificTest, PearsonNegativeCorrelation) {
    // pairs: (1,3),(2,2),(3,1) → r = -1.0
    auto r = plugin.call("pearson", {
        CellValue{1.0}, CellValue{3.0},
        CellValue{2.0}, CellValue{2.0},
        CellValue{3.0}, CellValue{1.0},
    });
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), -1.0, 1e-12);
}

TEST_F(ScientificTest, PearsonOddArgs) {
    auto r = plugin.call("pearson", {
        CellValue{1.0}, CellValue{2.0}, CellValue{3.0},
        CellValue{4.0}, CellValue{5.0},
    });
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::VALUE);
}

// ── unknown function ───────────────────────────────────────────────────────

TEST_F(ScientificTest, UnknownFunction) {
    auto r = plugin.call("nonexistent", {CellValue{1.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}
