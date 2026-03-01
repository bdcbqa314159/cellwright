#include <gtest/gtest.h>
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"

using namespace magic;

TEST(Parser, Number) {
    auto tokens = Tokenizer::tokenize("42");
    auto ast = Parser::parse(tokens);
    ASSERT_NE(ast, nullptr);
    EXPECT_TRUE(std::holds_alternative<NumberNode>(ast->value));
    EXPECT_DOUBLE_EQ(std::get<NumberNode>(ast->value).value, 42.0);
}

TEST(Parser, BinOp) {
    auto tokens = Tokenizer::tokenize("1+2");
    auto ast = Parser::parse(tokens);
    ASSERT_NE(ast, nullptr);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(ast->value));
    auto& binop = std::get<BinOpNode>(ast->value);
    EXPECT_EQ(binop.op, '+');
}

TEST(Parser, Precedence) {
    // 1+2*3 should be 1+(2*3)
    auto tokens = Tokenizer::tokenize("1+2*3");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(ast->value));
    auto& add = std::get<BinOpNode>(ast->value);
    EXPECT_EQ(add.op, '+');
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(add.right->value));
    auto& mul = std::get<BinOpNode>(add.right->value);
    EXPECT_EQ(mul.op, '*');
}

TEST(Parser, CellRef) {
    auto tokens = Tokenizer::tokenize("A1");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<CellRefNode>(ast->value));
    auto& ref = std::get<CellRefNode>(ast->value);
    EXPECT_EQ(ref.addr.col, 0);
    EXPECT_EQ(ref.addr.row, 0);
}

TEST(Parser, Range) {
    auto tokens = Tokenizer::tokenize("A1:B5");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<RangeNode>(ast->value));
}

TEST(Parser, FunctionCall) {
    auto tokens = Tokenizer::tokenize("SUM(1, 2, 3)");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<FuncCallNode>(ast->value));
    auto& func = std::get<FuncCallNode>(ast->value);
    EXPECT_EQ(func.name, "SUM");
    EXPECT_EQ(func.args.size(), 3u);
}

TEST(Parser, Nested) {
    auto tokens = Tokenizer::tokenize("SUM(A1:A10) + MAX(B1:B10)");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<BinOpNode>(ast->value));
}

TEST(Parser, Comparison) {
    auto tokens = Tokenizer::tokenize("A1>0");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<CompareNode>(ast->value));
    auto& cmp = std::get<CompareNode>(ast->value);
    EXPECT_EQ(cmp.op, ">");
}

TEST(Parser, UnaryMinus) {
    auto tokens = Tokenizer::tokenize("-5");
    auto ast = Parser::parse(tokens);
    ASSERT_TRUE(std::holds_alternative<UnaryOpNode>(ast->value));
}
