#pragma once
#include "plugin/IFunction.hpp"

namespace magic::plugins {

class ExampleStats : public magic::IFunction {
public:
    std::string name() const override { return "ExampleStats"; }
    std::string version() const override { return "1.0.0"; }
    std::string type() const override { return "function"; }

    std::vector<FunctionDescriptor> describe_functions() const override {
        return {{"ZSCORE", 3, 3}};
    }

    CellValue call(const std::string& func_name,
                   const std::vector<CellValue>& args) const override;
};

}  // namespace magic::plugins
