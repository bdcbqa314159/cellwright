#include <gtest/gtest.h>
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"

using namespace magic;

TEST(Parser, Number) {
    auto tokens = Tokenizer::tokenize("42");
    auto formula = Parser::parse(tokens);
    ASSERT_NE(formula.root, nullptr);
    EXPECT_TRUE(std::holds_alternative<NumberNode>(formula.root->value));
    EXPECT_DOUBLE_EQ(std::get<NumberNode>(formula.root->value).value, 42.0);
}

TEST(Parser, BinOp) {
    auto tokens = Tokenizer::tokenize("1+2");
    auto formula = Parser::parse(tokens);
    ASSERT_NE(formula.root, nullptr);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(formula.root->value));
    auto& binop = std::get<BinOpNode>(formula.root->value);
    EXPECT_EQ(binop.op, '+');
}

TEST(Parser, Precedence) {
    // 1+2*3 should be 1+(2*3)
    auto tokens = Tokenizer::tokenize("1+2*3");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(formula.root->value));
    auto& add = std::get<BinOpNode>(formula.root->value);
    EXPECT_EQ(add.op, '+');
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(add.right->value));
    auto& mul = std::get<BinOpNode>(add.right->value);
    EXPECT_EQ(mul.op, '*');
}

TEST(Parser, CellRef) {
    auto tokens = Tokenizer::tokenize("A1");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<CellRefNode>(formula.root->value));
    auto& ref = std::get<CellRefNode>(formula.root->value);
    EXPECT_EQ(ref.addr.col, 0);
    EXPECT_EQ(ref.addr.row, 0);
}

TEST(Parser, Range) {
    auto tokens = Tokenizer::tokenize("A1:B5");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<RangeNode>(formula.root->value));
}

TEST(Parser, FunctionCall) {
    auto tokens = Tokenizer::tokenize("SUM(1, 2, 3)");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<FuncCallNode>(formula.root->value));
    auto& func = std::get<FuncCallNode>(formula.root->value);
    EXPECT_EQ(func.name, "SUM");
    EXPECT_EQ(func.args.size(), 3u);
}

TEST(Parser, Nested) {
    auto tokens = Tokenizer::tokenize("SUM(A1:A10) + MAX(B1:B10)");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(formula.root->value));
}

TEST(Parser, Comparison) {
    auto tokens = Tokenizer::tokenize("A1>0");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<CompareNode>(formula.root->value));
    auto& cmp = std::get<CompareNode>(formula.root->value);
    EXPECT_EQ(cmp.op, ">");
}

TEST(Parser, UnaryMinus) {
    auto tokens = Tokenizer::tokenize("-5");
    auto formula = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<UnaryOpNode>(formula.root->value));
}
