#include "formula/FunctionRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace magic {

void FunctionRegistry::register_function(const std::string& name, SpreadsheetFunc fn) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    funcs_[upper] = std::move(fn);
    signatures_.erase(upper);  // clear stale hint if re-registering without one
}

void FunctionRegistry::register_function(const std::string& name, SpreadsheetFunc fn,
                                          const std::string& signature_hint) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    funcs_[upper] = std::move(fn);
    signatures_[upper] = signature_hint;
}

const std::string& FunctionRegistry::signature(const std::string& name) const {
    static const std::string empty;
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    auto it = signatures_.find(upper);
    return it != signatures_.end() ? it->second : empty;
}

bool FunctionRegistry::unregister_function(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    signatures_.erase(upper);
    return funcs_.erase(upper) > 0;
}

bool FunctionRegistry::has(const std::string& name) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    return funcs_.count(upper) > 0;
}

CellValue FunctionRegistry::call(const std::string& name, const std::vector<CellValue>& args) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    auto it = funcs_.find(upper);
    if (it == funcs_.end())
        return CellValue{CellError::NAME};
    return it->second(args);
}

CellValue FunctionRegistry::call_direct(const std::string& name, const std::vector<CellValue>& args) const {
    auto it = funcs_.find(name);
    if (it == funcs_.end())
        return CellValue{CellError::NAME};
    return it->second(args);
}

void FunctionRegistry::clear() {
    funcs_.clear();
    signatures_.clear();
}

}  // namespace magic
