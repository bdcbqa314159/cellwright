#pragma once
#include <string>

namespace magic {

enum class TokenType {
    NUMBER,      // 3.14
    STRING,      // "hello"
    CELLREF,     // A1
    RANGE,       // A1:B10 (represented as two CELLREFs with COLON between)
    COLON,       // :
    FUNC,        // SUM, IF, etc. (identifier followed by '(')
    IDENT,       // TRUE, FALSE, or unknown identifier
    LPAREN,      // (
    RPAREN,      // )
    COMMA,       // ,
    PLUS,        // +
    MINUS,       // -
    STAR,        // *
    SLASH,       // /
    CARET,       // ^
    EQ,          // =
    NEQ,         // <>
    LT,          // <
    GT,          // >
    LTE,         // <=
    GTE,         // >=
    END,
};

struct Token {
    TokenType type;
    std::string text;
    double number_value = 0.0;
};

}  // namespace magic
