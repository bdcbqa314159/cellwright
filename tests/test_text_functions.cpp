#include <gtest/gtest.h>
#include "core/Sheet.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/FunctionRegistry.hpp"
#include "builtin/TextFunctions.hpp"
#include "builtin/StatFunctions.hpp"

using namespace magic;

class FuncTest : public ::testing::Test {
protected:
    void SetUp() override {
        register_text_functions(registry);
        register_stat_functions(registry);
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

TEST_F(FuncTest, Len) {
    sheet.set_value({0, 0}, CellValue{std::string("hello")});
    auto r = evaluate("LEN(A1)");
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 5.0);
}

TEST_F(FuncTest, Upper) {
    sheet.set_value({0, 0}, CellValue{std::string("hello")});
    auto r = evaluate("UPPER(A1)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "HELLO");
}

TEST_F(FuncTest, Lower) {
    sheet.set_value({0, 0}, CellValue{std::string("HELLO")});
    auto r = evaluate("LOWER(A1)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "hello");
}

TEST_F(FuncTest, Left) {
    sheet.set_value({0, 0}, CellValue{std::string("hello")});
    auto r = evaluate("LEFT(A1, 3)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "hel");
}

TEST_F(FuncTest, Right) {
    sheet.set_value({0, 0}, CellValue{std::string("hello")});
    auto r = evaluate("RIGHT(A1, 2)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "lo");
}

TEST_F(FuncTest, Mid) {
    sheet.set_value({0, 0}, CellValue{std::string("hello world")});
    auto r = evaluate("MID(A1, 7, 5)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "world");
}

TEST_F(FuncTest, Concatenate) {
    sheet.set_value({0, 0}, CellValue{std::string("hello")});
    sheet.set_value({1, 0}, CellValue{std::string(" world")});
    auto r = evaluate("CONCATENATE(A1, B1)");
    ASSERT_TRUE(is_string(r));
    EXPECT_EQ(as_string(r), "hello world");
}

TEST_F(FuncTest, Stdev) {
    for (int i = 0; i < 5; ++i)
        sheet.set_value({0, i}, CellValue{static_cast<double>(i + 1)});
    auto r = evaluate("STDEV(A1:A5)");
    ASSERT_TRUE(is_number(r));
    EXPECT_NEAR(as_number(r), 1.5811, 0.001);
}

TEST_F(FuncTest, Median) {
    sheet.set_value({0, 0}, CellValue{1.0});
    sheet.set_value({0, 1}, CellValue{3.0});
    sheet.set_value({0, 2}, CellValue{5.0});
    auto r = evaluate("MEDIAN(A1:A3)");
    ASSERT_TRUE(is_number(r));
    EXPECT_DOUBLE_EQ(as_number(r), 3.0);
}
