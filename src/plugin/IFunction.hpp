#pragma once
#include <IPlugin.hpp>
#include <string>
#include <vector>

namespace magic {

struct FunctionDescriptor {
    std::string name;
    int arg_count;  // -1 = variadic
};

// Plugin interface for spreadsheet functions.
// A single plugin can provide multiple functions.
class IFunction : public plugin_arch::IPlugin {
public:
    virtual std::vector<FunctionDescriptor> describe_functions() const = 0;

    // Call a function by name with double arguments, return double.
    // For richer types, use the FunctionRegistry directly.
    virtual double call(const std::string& func_name, const std::vector<double>& args) const = 0;
};

}  // namespace magic
