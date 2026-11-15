#pragma once
#include <string>
#include <vector>

#include "formula/Token.hpp"

namespace magic {

class Tokenizer {
public:
    // Tokenize a formula string (without leading '=')
    static std::vector<Token> tokenize(const std::string& formula);
};

}  // namespace magic
