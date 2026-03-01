#pragma once
#include <IPlugin.hpp>
#include "core/CellValue.hpp"
#include <string>
#include <vector>

namespace magic {

struct FunctionDescriptor {
    std::string name;
    int min_args = 0;   // minimum required arguments
    int max_args = -1;  // -1 = unlimited/variadic
};

// Plugin interface for spreadsheet functions.
// A single plugin can provide multiple functions.
class IFunction : public plugin_arch::IPlugin {
public:
    virtual std::vector<FunctionDescriptor> describe_functions() const = 0;

    // Call a function by name with CellValue arguments, return CellValue.
    virtual CellValue call(const std::string& func_name,
                           const std::vector<CellValue>& args) const = 0;
};

}  // namespace magic
