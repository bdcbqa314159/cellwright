#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <optional>

namespace magic {

struct CellAddress {
    int32_t col = 0;  // 0-based
    int32_t row = 0;  // 0-based

    bool operator==(const CellAddress&) const = default;
    bool operator<(const CellAddress& o) const {
        return (col != o.col) ? col < o.col : row < o.row;
    }

    // "B3" → {1, 2}
    static std::optional<CellAddress> from_a1(const std::string& s);

    // {1, 2} → "B3"
    std::string to_a1() const;

    // Column index → letter(s): 0→A, 25→Z, 26→AA
    static std::string col_to_letters(int32_t c);

    // Letter(s) → column index: A→0, Z→25, AA→26
    static int32_t letters_to_col(const std::string& letters);
};

}  // namespace magic

template <>
struct std::hash<magic::CellAddress> {
    std::size_t operator()(const magic::CellAddress& a) const noexcept {
        auto h1 = std::hash<int32_t>{}(a.col);
        auto h2 = std::hash<int32_t>{}(a.row);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL);
    }
};
