#pragma once
#include "core/CellValue.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace magic {

// A spreadsheet function takes a list of CellValue arguments and returns a CellValue.
using SpreadsheetFunc = std::function<CellValue(const std::vector<CellValue>&)>;

class FunctionRegistry {
public:
    void register_function(const std::string& name, SpreadsheetFunc fn);
    bool has(const std::string& name) const;
    CellValue call(const std::string& name, const std::vector<CellValue>& args) const;

    const std::unordered_map<std::string, SpreadsheetFunc>& all() const { return funcs_; }

private:
    std::unordered_map<std::string, SpreadsheetFunc> funcs_;
};

}  // namespace magic
