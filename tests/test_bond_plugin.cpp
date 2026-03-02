#include <gtest/gtest.h>
#include "../plugins/bond/bond_plugin.hpp"
#include "core/CellValue.hpp"
using namespace magic;

class BondTest : public ::testing::Test {
protected:
    magic::plugins::BondPlugin plugin;

    std::string makeBond(double coupon, double ytm, double maturity, double freq) {
        auto r = plugin.call("bond", {
            CellValue{coupon}, CellValue{ytm}, CellValue{maturity}, CellValue{freq}
        });
        EXPECT_TRUE(is_string(r));
        return as_string(r);
    }
};

// ── par bond (coupon == ytm -> price == 100) ────────────────────────────────

TEST_F(BondTest, ParBondPrice) {
    auto h = makeBond(0.05, 0.05, 10, 2);
    auto r = plugin.call("bondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 100.0, 1e-9);
}

// ── premium bond (coupon > ytm -> price > 100) ─────────────────────────────

TEST_F(BondTest, PremiumBondPrice) {
    auto h = makeBond(0.05, 0.035, 10, 2);
    auto r = plugin.call("bondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_GT(as_number(r), 100.0);
}

// ── discount bond (coupon < ytm -> price < 100) ────────────────────────────

TEST_F(BondTest, DiscountBondPrice) {
    auto h = makeBond(0.03, 0.05, 10, 2);
    auto r = plugin.call("bondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_LT(as_number(r), 100.0);
}

// ── duration sanity ─────────────────────────────────────────────────────────

TEST_F(BondTest, DurationPositiveAndLessThanMaturity) {
    auto h = makeBond(0.05, 0.05, 10, 2);
    auto r = plugin.call("bondDuration", {CellValue{h}});
    ASSERT_TRUE(is_number(r));
    EXPECT_GT(as_number(r), 0.0);
    EXPECT_LT(as_number(r), 10.0);
}

// ── handle round-trip (bond -> bondPrice/bondYtm/bondCoupon) ────────────────

TEST_F(BondTest, HandleRoundTrip) {
    auto h = makeBond(0.06, 0.04, 5, 2);
    EXPECT_TRUE(h.starts_with("Bond#"));

    auto price = plugin.call("bondPrice", {CellValue{h}});
    ASSERT_TRUE(is_number(price));
    EXPECT_GT(as_number(price), 100.0);

    auto ytm = plugin.call("bondYtm", {CellValue{h}});
    ASSERT_TRUE(is_number(ytm));
    EXPECT_DOUBLE_EQ(as_number(ytm), 0.04);

    auto coupon = plugin.call("bondCoupon", {CellValue{h}});
    ASSERT_TRUE(is_number(coupon));
    EXPECT_DOUBLE_EQ(as_number(coupon), 0.06);
}

// ── unknown handle -> NAME error ────────────────────────────────────────────

TEST_F(BondTest, UnknownHandle) {
    auto r = plugin.call("bondPrice", {CellValue{std::string("Bond#9999")}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}

// ── unknown function ────────────────────────────────────────────────────────

TEST_F(BondTest, UnknownFunction) {
    auto r = plugin.call("nonexistent", {CellValue{1.0}});
    ASSERT_TRUE(is_error(r));
    EXPECT_EQ(std::get<CellError>(r), CellError::NAME);
}
