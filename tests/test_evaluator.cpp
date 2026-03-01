#include <gtest/gtest.h>
#include "core/Sheet.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/FunctionRegistry.hpp"
#include "builtin/MathFunctions.hpp"
#include "builtin/LogicFunctions.hpp"

using namespace magic;

class EvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        register_math_functions(registry);
        register_logic_functions(registry);
    }

    CellValue evaluate(const std::string& formula) {
        auto tokens = Tokenizer::tokenize(formula);
        auto ast = Parser::parse(tokens);
        Evaluator eval(sheet, registry);
        return eval.evaluate(*ast);
    }

    Sheet sheet{"Test"};
    FunctionRegistry registry;
};

TEST_F(EvaluatorTest, SimpleAdd) {
    sheet.set_value({0, 0}, CellValue{10.0});  // A1
    sheet.set_value({1, 0}, CellValue{20.0});  // B1
    auto result = evaluate("A1+B1");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 30.0);
}

TEST_F(EvaluatorTest, Multiply) {
    sheet.set_value({0, 0}, CellValue{5.0});
    sheet.set_value({1, 0}, CellValue{3.0});
    auto result = evaluate("A1*B1");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 15.0);
}

TEST_F(EvaluatorTest, DivByZero) {
    sheet.set_value({0, 0}, CellValue{10.0});
    sheet.set_value({1, 0}, CellValue{0.0});
    auto result = evaluate("A1/B1");
    ASSERT_TRUE(is_error(result));
    EXPECT_EQ(std::get<CellError>(result), CellError::DIV0);
}

TEST_F(EvaluatorTest, SumRange) {
    for (int i = 0; i < 10; ++i) {
        sheet.set_value({0, i}, CellValue{static_cast<double>(i + 1)});
    }
    auto result = evaluate("SUM(A1:A10)");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 55.0);
}

TEST_F(EvaluatorTest, Average) {
    sheet.set_value({0, 0}, CellValue{10.0});
    sheet.set_value({0, 1}, CellValue{20.0});
    sheet.set_value({0, 2}, CellValue{30.0});
    auto result = evaluate("AVERAGE(A1:A3)");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 20.0);
}

TEST_F(EvaluatorTest, IfTrue) {
    sheet.set_value({0, 0}, CellValue{10.0});
    auto result = evaluate("IF(A1>0, \"pos\", \"neg\")");
    ASSERT_TRUE(is_string(result));
    EXPECT_EQ(as_string(result), "pos");
}

TEST_F(EvaluatorTest, IfFalse) {
    sheet.set_value({0, 0}, CellValue{-5.0});
    auto result = evaluate("IF(A1>0, \"pos\", \"neg\")");
    ASSERT_TRUE(is_string(result));
    EXPECT_EQ(as_string(result), "neg");
}

TEST_F(EvaluatorTest, NestedFunctions) {
    for (int i = 0; i < 5; ++i) {
        sheet.set_value({0, i}, CellValue{static_cast<double>(i + 1)});
    }
    auto result = evaluate("SUM(A1:A5) + MAX(A1:A5)");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 20.0);  // 15 + 5
}

TEST_F(EvaluatorTest, Power) {
    auto result = evaluate("2^10");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 1024.0);
}

TEST_F(EvaluatorTest, UnaryMinus) {
    sheet.set_value({0, 0}, CellValue{5.0});
    auto result = evaluate("-A1");
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), -5.0);
}
