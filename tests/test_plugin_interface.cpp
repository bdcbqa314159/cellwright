#include <gtest/gtest.h>
#include "plugin/IFunction.hpp"
#include "plugin/PluginManager.hpp"
#include "formula/FunctionRegistry.hpp"
#include "core/CellValue.hpp"
#include <cmath>

using namespace magic;

// ── Mock plugin implementing IFunction ──────────────────────────────────────

class MockStats : public IFunction {
public:
    std::string name() const override { return "MockStats"; }
    std::string version() const override { return "1.0.0"; }
    std::string type() const override { return "function"; }

    std::vector<FunctionDescriptor> describe_functions() const override {
        return {{"ZSCORE", 3, 3}};
    }

    CellValue call(const std::string& func_name,
                   const std::vector<CellValue>& args) const override {
        if (func_name == "ZSCORE") {
            double x = to_double(args[0]);
            double mean = to_double(args[1]);
            double stddev = to_double(args[2]);
            if (stddev == 0.0) return CellValue{CellError::DIV0};
            return CellValue{(x - mean) / stddev};
        }
        return CellValue{CellError::NAME};
    }
};

// ── Direct plugin tests (no dynamic loading) ───────────────────────────────

class PluginInterfaceTest : public ::testing::Test {
protected:
    MockStats plugin;
};

TEST_F(PluginInterfaceTest, CorrectCall) {
    std::vector<CellValue> args = {CellValue{85.0}, CellValue{70.0}, CellValue{10.0}};
    auto result = plugin.call("ZSCORE", args);
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 1.5);
}

TEST_F(PluginInterfaceTest, CellRefDoubles) {
    // CellValue{double} simulates resolved cell references
    std::vector<CellValue> args = {CellValue{100.0}, CellValue{50.0}, CellValue{25.0}};
    auto result = plugin.call("ZSCORE", args);
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 2.0);
}

TEST_F(PluginInterfaceTest, StringArgsCoerce) {
    // to_double coerces strings; non-numeric strings → NaN
    std::vector<CellValue> args = {CellValue{std::string("85")}, CellValue{70.0}, CellValue{10.0}};
    auto result = plugin.call("ZSCORE", args);
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 1.5);
}

TEST_F(PluginInterfaceTest, ErrorArgPropagation) {
    // CellError in args → to_double returns NaN → result is NaN
    std::vector<CellValue> args = {CellValue{CellError::REF}, CellValue{70.0}, CellValue{10.0}};
    auto result = plugin.call("ZSCORE", args);
    ASSERT_TRUE(is_number(result));
    EXPECT_TRUE(std::isnan(as_number(result)));
}

TEST_F(PluginInterfaceTest, DivByZero) {
    std::vector<CellValue> args = {CellValue{85.0}, CellValue{70.0}, CellValue{0.0}};
    auto result = plugin.call("ZSCORE", args);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(std::get<CellError>(result), CellError::DIV0);
}

TEST_F(PluginInterfaceTest, UnknownFunction) {
    std::vector<CellValue> args = {CellValue{1.0}, CellValue{2.0}, CellValue{3.0}};
    auto result = plugin.call("NONEXISTENT", args);
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(std::get<CellError>(result), CellError::NAME);
}

// ── Arg validation via PluginManager wrapper ────────────────────────────────

class PluginWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock = std::make_shared<MockStats>();
        for (const auto& fd : mock->describe_functions()) {
            auto inst = mock;
            std::string fn_name = fd.name;
            int min_args = fd.min_args;
            int max_args = fd.max_args;
            registry.register_function(fd.name,
                [inst, fn_name, min_args, max_args](const std::vector<CellValue>& args) -> CellValue {
                    int n = static_cast<int>(args.size());
                    if (n < min_args) return CellValue{CellError::VALUE};
                    if (max_args >= 0 && n > max_args) return CellValue{CellError::VALUE};
                    return inst->call(fn_name, args);
                });
        }
    }

    FunctionRegistry registry;
};

TEST_F(PluginWrapperTest, TooFewArgs) {
    auto result = registry.call("ZSCORE", {CellValue{1.0}});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(std::get<CellError>(result), CellError::VALUE);
}

TEST_F(PluginWrapperTest, TooManyArgs) {
    auto result = registry.call("ZSCORE",
        {CellValue{1.0}, CellValue{2.0}, CellValue{3.0}, CellValue{4.0}});
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(std::get<CellError>(result), CellError::VALUE);
}

TEST_F(PluginWrapperTest, ExactArgCount) {
    auto result = registry.call("ZSCORE",
        {CellValue{85.0}, CellValue{70.0}, CellValue{10.0}});
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 1.5);
}

// ── C ABI signature parsing ────────────────────────────────────────────────

TEST(CAbiSignatureTest, TwoArgs) {
    EXPECT_EQ(count_args_from_signature("double(double, double)"), 2);
}

TEST(CAbiSignatureTest, ZeroArgs) {
    EXPECT_EQ(count_args_from_signature("double()"), 0);
}

TEST(CAbiSignatureTest, OneArg) {
    EXPECT_EQ(count_args_from_signature("double(double)"), 1);
}

TEST(CAbiSignatureTest, ThreeArgs) {
    EXPECT_EQ(count_args_from_signature("double(double, double, double)"), 3);
}

TEST(CAbiSignatureTest, FourArgs) {
    EXPECT_EQ(count_args_from_signature("int(int, int, int, int)"), 4);
}

TEST(CAbiSignatureTest, VoidSignature) {
    EXPECT_EQ(count_args_from_signature("void()"), 0);
}
