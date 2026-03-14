#include "core/CellAddress.hpp"
#include <cctype>

namespace magic {

std::string CellAddress::col_to_letters(int32_t c) {
    std::string result;
    do {
        result.insert(result.begin(), static_cast<char>('A' + (c % 26)));
        c = c / 26 - 1;
    } while (c >= 0);
    return result;
}

int32_t CellAddress::letters_to_col(const std::string& letters) {
    int32_t col = 0;
    for (char ch : letters) {
        col = col * 26 + (std::toupper(ch) - 'A' + 1);
    }
    return col - 1;
}

std::optional<CellAddress> CellAddress::from_a1(const std::string& s) {
    if (s.empty()) return std::nullopt;

    size_t i = 0;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
    if (i == 0 || i == s.size()) return std::nullopt;

    std::string letters = s.substr(0, i);
    std::string digits = s.substr(i);
    for (char ch : digits) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return std::nullopt;
    }

    int row_1based;
    try { row_1based = std::stoi(digits); }
    catch (...) { return std::nullopt; }
    static constexpr int MAX_ROW = 1048576;
    if (row_1based < 1 || row_1based > MAX_ROW) return std::nullopt;

    return CellAddress{letters_to_col(letters), row_1based - 1};
}

std::string CellAddress::to_a1() const {
    return col_to_letters(col) + std::to_string(row + 1);
}

}  // namespace magic
