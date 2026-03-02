#include "formula/FunctionRegistry.hpp"
#include <algorithm>
#include <stdexcept>

namespace magic {

void FunctionRegistry::register_function(const std::string& name, SpreadsheetFunc fn) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    funcs_[upper] = std::move(fn);
}

bool FunctionRegistry::has(const std::string& name) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return funcs_.count(upper) > 0;
}

CellValue FunctionRegistry::call(const std::string& name, const std::vector<CellValue>& args) const {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
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
}

}  // namespace magic
