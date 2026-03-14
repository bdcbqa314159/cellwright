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
    void register_function(const std::string& name, SpreadsheetFunc fn,
                           const std::string& signature_hint);
    [[nodiscard]] bool unregister_function(const std::string& name);
    bool has(const std::string& name) const;
    CellValue call(const std::string& name, const std::vector<CellValue>& args) const;
    // Like call(), but assumes name is already uppercase (skips toupper transform).
    CellValue call_direct(const std::string& name, const std::vector<CellValue>& args) const;
    void clear();

    const std::unordered_map<std::string, SpreadsheetFunc>& all() const { return funcs_; }

    // Returns the signature hint for a function (e.g., "SUM(number1, ...)").
    // Returns empty string if no hint is registered.
    [[nodiscard]] std::string signature(const std::string& name) const;

private:
    std::unordered_map<std::string, SpreadsheetFunc> funcs_;
    std::unordered_map<std::string, std::string> signatures_;
};

}  // namespace magic
