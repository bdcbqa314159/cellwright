#pragma once
#include "formula/Token.hpp"
#include <string>
#include <vector>

namespace magic {

class Tokenizer {
public:
    // Tokenize a formula string (without leading '=')
    static std::vector<Token> tokenize(const std::string& formula);
};

}  // namespace magic
