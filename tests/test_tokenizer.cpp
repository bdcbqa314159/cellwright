#include <gtest/gtest.h>
#include "formula/Tokenizer.hpp"

using namespace magic;

TEST(Tokenizer, Number) {
    auto tokens = Tokenizer::tokenize("42");
    ASSERT_EQ(tokens.size(), 2u);  // NUMBER + END
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_DOUBLE_EQ(tokens[0].number_value, 42.0);
}

TEST(Tokenizer, CellRef) {
    auto tokens = Tokenizer::tokenize("A1");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[0].text, "A1");
}

TEST(Tokenizer, FunctionCall) {
    auto tokens = Tokenizer::tokenize("SUM(A1, B2)");
    ASSERT_GE(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].type, TokenType::FUNC);
    EXPECT_EQ(tokens[0].text, "SUM");
    EXPECT_EQ(tokens[1].type, TokenType::LPAREN);
    EXPECT_EQ(tokens[2].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[3].type, TokenType::COMMA);
    EXPECT_EQ(tokens[4].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[5].type, TokenType::RPAREN);
}

TEST(Tokenizer, Operators) {
    auto tokens = Tokenizer::tokenize("A1+B1*2");
    ASSERT_EQ(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[1].type, TokenType::PLUS);
    EXPECT_EQ(tokens[2].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[3].type, TokenType::STAR);
    EXPECT_EQ(tokens[4].type, TokenType::NUMBER);
}

TEST(Tokenizer, Range) {
    auto tokens = Tokenizer::tokenize("A1:B10");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[1].type, TokenType::COLON);
    EXPECT_EQ(tokens[2].type, TokenType::CELLREF);
}

TEST(Tokenizer, Comparison) {
    auto tokens = Tokenizer::tokenize("A1<=B1");
    ASSERT_GE(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, TokenType::CELLREF);
    EXPECT_EQ(tokens[1].type, TokenType::LTE);
    EXPECT_EQ(tokens[2].type, TokenType::CELLREF);
}

TEST(Tokenizer, StringLiteral) {
    auto tokens = Tokenizer::tokenize("\"hello\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].text, "hello");
}
