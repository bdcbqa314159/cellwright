#pragma once
#include "plugin/IFunction.hpp"

namespace magic::plugins {

class BondPlugin : public magic::IFunction {
public:
    const std::string& name() const override { return name_; }
    const std::string& version() const override { return version_; }
    const std::string& type() const override { return type_; }

    std::vector<FunctionDescriptor> describe_functions() const override {
        return {
            {"bond",         4, 4},
            {"bondPrice",    1, 1},
            {"bondYtm",      1, 1},
            {"bondCoupon",   1, 1},
            {"bondDuration", 1, 1},
        };
    }

    CellValue call(const std::string& func_name,
                   const std::vector<CellValue>& args) const override;

private:
    std::string name_    = "BondPlugin";
    std::string version_ = "1.0.0";
    std::string type_    = "function";
};

}  // namespace magic::plugins
