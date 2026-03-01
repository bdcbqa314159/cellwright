#include "formula/Parser.hpp"
#include "core/CellAddress.hpp"
#include <algorithm>
#include <stdexcept>

namespace magic {

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

ASTNodePtr Parser::parse(const std::vector<Token>& tokens) {
    Parser p(tokens);
    return p.parse_expression();
}

const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::advance() {
    return tokens_[pos_++];
}

bool Parser::match(TokenType type) {
    if (current().type == type) {
        advance();
        return true;
    }
    return false;
}

void Parser::expect(TokenType type) {
    if (!match(type))
        throw std::runtime_error("Expected token type " + std::to_string(static_cast<int>(type)));
}

// expression → comparison
ASTNodePtr Parser::parse_expression() {
    return parse_comparison();
}

// comparison → addition ( (= | <> | < | > | <= | >=) addition )?
ASTNodePtr Parser::parse_comparison() {
    auto left = parse_addition();

    auto t = current().type;
    if (t == TokenType::EQ || t == TokenType::NEQ ||
        t == TokenType::LT || t == TokenType::GT ||
        t == TokenType::LTE || t == TokenType::GTE) {
        std::string op = current().text;
        advance();
        auto right = parse_addition();
        return make_node(CompareNode{op, std::move(left), std::move(right)});
    }

    return left;
}

// addition → multiplication ( (+|-) multiplication )*
ASTNodePtr Parser::parse_addition() {
    auto left = parse_multiplication();

    while (current().type == TokenType::PLUS || current().type == TokenType::MINUS) {
        char op = current().text[0];
        advance();
        auto right = parse_multiplication();
        left = make_node(BinOpNode{op, std::move(left), std::move(right)});
    }

    return left;
}

// multiplication → power ( (*|/) power )*
ASTNodePtr Parser::parse_multiplication() {
    auto left = parse_power();

    while (current().type == TokenType::STAR || current().type == TokenType::SLASH) {
        char op = current().text[0];
        advance();
        auto right = parse_power();
        left = make_node(BinOpNode{op, std::move(left), std::move(right)});
    }

    return left;
}

// power → unary ( ^ unary )*
ASTNodePtr Parser::parse_power() {
    auto left = parse_unary();

    while (current().type == TokenType::CARET) {
        advance();
        auto right = parse_unary();
        left = make_node(BinOpNode{'^', std::move(left), std::move(right)});
    }

    return left;
}

// unary → (-|+) unary | atom
ASTNodePtr Parser::parse_unary() {
    if (current().type == TokenType::MINUS) {
        advance();
        auto operand = parse_unary();
        return make_node(UnaryOpNode{'-', std::move(operand)});
    }
    if (current().type == TokenType::PLUS) {
        advance();
        return parse_unary();
    }
    return parse_atom();
}

// atom → NUMBER | STRING | CELLREF (:CELLREF)? | FUNC(args) | IDENT | (expression)
ASTNodePtr Parser::parse_atom() {
    // Number
    if (current().type == TokenType::NUMBER) {
        double val = current().number_value;
        advance();
        return make_node(NumberNode{val});
    }

    // String
    if (current().type == TokenType::STRING) {
        std::string val = current().text;
        advance();
        return make_node(StringNode{std::move(val)});
    }

    // Sheet reference: Sheet2!A1 or Sheet2!A1:B10
    if (current().type == TokenType::SHEETREF) {
        std::string sheet_name = current().text;
        advance();

        if (current().type != TokenType::CELLREF)
            throw std::runtime_error("Expected cell reference after '" + sheet_name + "!'");
        std::string ref_text = current().text;
        advance();
        auto addr = CellAddress::from_a1(ref_text);
        if (!addr) throw std::runtime_error("Invalid cell reference: " + ref_text);

        if (current().type == TokenType::COLON) {
            advance();
            if (current().type != TokenType::CELLREF)
                throw std::runtime_error("Expected cell reference after ':'");
            std::string ref2_text = current().text;
            advance();
            auto addr2 = CellAddress::from_a1(ref2_text);
            if (!addr2) throw std::runtime_error("Invalid cell reference: " + ref2_text);
            return make_node(SheetRangeNode{sheet_name, *addr, *addr2});
        }

        return make_node(SheetRefNode{sheet_name, *addr});
    }

    // Cell reference (possibly range)
    if (current().type == TokenType::CELLREF) {
        std::string ref_text = current().text;
        advance();

        auto addr = CellAddress::from_a1(ref_text);
        if (!addr) throw std::runtime_error("Invalid cell reference: " + ref_text);

        // Check for range (A1:B10)
        if (current().type == TokenType::COLON) {
            advance();
            if (current().type != TokenType::CELLREF)
                throw std::runtime_error("Expected cell reference after ':'");
            std::string ref2_text = current().text;
            advance();
            auto addr2 = CellAddress::from_a1(ref2_text);
            if (!addr2) throw std::runtime_error("Invalid cell reference: " + ref2_text);
            return make_node(RangeNode{*addr, *addr2});
        }

        return make_node(CellRefNode{*addr});
    }

    // Function call
    if (current().type == TokenType::FUNC) {
        std::string name = current().text;
        // Uppercase the function name for case-insensitive lookup
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);
        advance();
        expect(TokenType::LPAREN);

        std::vector<ASTNodePtr> args;
        if (current().type != TokenType::RPAREN) {
            args.push_back(parse_expression());
            while (match(TokenType::COMMA)) {
                args.push_back(parse_expression());
            }
        }
        expect(TokenType::RPAREN);
        return make_node(FuncCallNode{std::move(name), std::move(args)});
    }

    // Boolean keywords
    if (current().type == TokenType::IDENT) {
        std::string text = current().text;
        std::string upper = text;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        advance();
        if (upper == "TRUE") return make_node(BoolNode{true});
        if (upper == "FALSE") return make_node(BoolNode{false});
        throw std::runtime_error("Unknown identifier: " + text);
    }

    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto node = parse_expression();
        expect(TokenType::RPAREN);
        return node;
    }

    throw std::runtime_error("Unexpected token: " + current().text);
}

}  // namespace magic
