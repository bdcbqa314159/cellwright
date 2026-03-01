#pragma once
#include "formula/ASTNode.hpp"
#include "formula/Token.hpp"
#include <vector>

namespace magic {

class Parser {
public:
    // Parse a token stream into an AST
    static ASTNodePtr parse(const std::vector<Token>& tokens);

private:
    explicit Parser(const std::vector<Token>& tokens);

    ASTNodePtr parse_expression();
    ASTNodePtr parse_comparison();
    ASTNodePtr parse_addition();
    ASTNodePtr parse_multiplication();
    ASTNodePtr parse_power();
    ASTNodePtr parse_unary();
    ASTNodePtr parse_atom();

    const Token& current() const;
    const Token& advance();
    bool match(TokenType type);
    void expect(TokenType type);

    const std::vector<Token>& tokens_;
    size_t pos_ = 0;
};

}  // namespace magic
