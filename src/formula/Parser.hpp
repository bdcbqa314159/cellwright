#pragma once
#include "formula/ASTNode.hpp"
#include "formula/Token.hpp"
#include <vector>

namespace magic {

class Parser {
public:
    // Parse a token stream into an AST owned by a ParsedFormula
    static ParsedFormula parse(const std::vector<Token>& tokens);

private:
    Parser(const std::vector<Token>& tokens, Arena& arena);

    ASTNode* parse_expression();
    ASTNode* parse_comparison();
    ASTNode* parse_addition();
    ASTNode* parse_multiplication();
    ASTNode* parse_power();
    ASTNode* parse_unary();
    ASTNode* parse_atom();

    const Token& current() const;
    const Token& advance();
    bool match(TokenType type);
    void expect(TokenType type);

    const std::vector<Token>& tokens_;
    size_t pos_ = 0;
    Arena& arena_;
};

}  // namespace magic
