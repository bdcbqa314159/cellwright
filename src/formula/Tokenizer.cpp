#include "formula/Tokenizer.hpp"
#include <cctype>
#include <stdexcept>

namespace magic {

static bool is_cell_ref_start(const std::string& s, size_t pos) {
    if (pos >= s.size() || !std::isalpha(static_cast<unsigned char>(s[pos])))
        return false;
    // Look for pattern: letters followed by digits
    size_t i = pos;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
    return i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]));
}

std::vector<Token> Tokenizer::tokenize(const std::string& formula) {
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = formula.size();

    while (i < n) {
        char ch = formula[i];

        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(ch))) { ++i; continue; }

        // Number
        if (std::isdigit(static_cast<unsigned char>(ch)) || (ch == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(formula[i + 1])))) {
            size_t start = i;
            while (i < n && (std::isdigit(static_cast<unsigned char>(formula[i])) || formula[i] == '.')) ++i;
            std::string text = formula.substr(start, i - start);
            tokens.push_back({TokenType::NUMBER, text, std::stod(text)});
            continue;
        }

        // String literal
        if (ch == '"') {
            size_t start = ++i;
            while (i < n && formula[i] != '"') ++i;
            std::string text = formula.substr(start, i - start);
            if (i < n) ++i;  // skip closing quote
            tokens.push_back({TokenType::STRING, text});
            continue;
        }

        // Identifier: cell ref, function name, or keyword
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(formula[i])) || formula[i] == '_')) ++i;
            std::string text = formula.substr(start, i - start);

            // Check if it's a function call (followed by '(')
            size_t j = i;
            while (j < n && std::isspace(static_cast<unsigned char>(formula[j]))) ++j;
            if (j < n && formula[j] == '(') {
                tokens.push_back({TokenType::FUNC, text});
                continue;
            }

            // Check if it looks like a cell reference (letters + digits)
            if (is_cell_ref_start(formula, start)) {
                // Verify the whole token is letters then digits
                size_t k = 0;
                while (k < text.size() && std::isalpha(static_cast<unsigned char>(text[k]))) ++k;
                bool all_digits = k < text.size();
                for (size_t m = k; m < text.size(); ++m) {
                    if (!std::isdigit(static_cast<unsigned char>(text[m]))) { all_digits = false; break; }
                }
                if (all_digits) {
                    tokens.push_back({TokenType::CELLREF, text});
                    continue;
                }
            }

            tokens.push_back({TokenType::IDENT, text});
            continue;
        }

        // Operators and punctuation
        switch (ch) {
            case '+': tokens.push_back({TokenType::PLUS, "+"}); ++i; continue;
            case '-': tokens.push_back({TokenType::MINUS, "-"}); ++i; continue;
            case '*': tokens.push_back({TokenType::STAR, "*"}); ++i; continue;
            case '/': tokens.push_back({TokenType::SLASH, "/"}); ++i; continue;
            case '^': tokens.push_back({TokenType::CARET, "^"}); ++i; continue;
            case '(': tokens.push_back({TokenType::LPAREN, "("}); ++i; continue;
            case ')': tokens.push_back({TokenType::RPAREN, ")"}); ++i; continue;
            case ',': tokens.push_back({TokenType::COMMA, ","}); ++i; continue;
            case ':': tokens.push_back({TokenType::COLON, ":"}); ++i; continue;
            case '=': tokens.push_back({TokenType::EQ, "="}); ++i; continue;
            case '<':
                if (i + 1 < n && formula[i + 1] == '=') {
                    tokens.push_back({TokenType::LTE, "<="}); i += 2;
                } else if (i + 1 < n && formula[i + 1] == '>') {
                    tokens.push_back({TokenType::NEQ, "<>"}); i += 2;
                } else {
                    tokens.push_back({TokenType::LT, "<"}); ++i;
                }
                continue;
            case '>':
                if (i + 1 < n && formula[i + 1] == '=') {
                    tokens.push_back({TokenType::GTE, ">="}); i += 2;
                } else {
                    tokens.push_back({TokenType::GT, ">"}); ++i;
                }
                continue;
            default:
                throw std::runtime_error(std::string("Unexpected character: ") + ch);
        }
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}

}  // namespace magic
