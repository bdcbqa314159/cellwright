#pragma once
#include "plugin/IFunction.hpp"

namespace magic::plugins {

class Scientific : public magic::IFunction {
public:
    const std::string& name() const override { return name_; }
    const std::string& version() const override { return version_; }
    const std::string& type() const override { return type_; }

    std::vector<FunctionDescriptor> describe_functions() const override {
        return {
            {"sigmoid",    1, 1},
            {"logit",      1, 1},
            {"entropy",    1, -1},   // variadic: 1+ probability values
            {"decay",      3, 3},
            {"halflife",   3, 3},
            {"blackbody",  2, 2},
            {"doppler",    2, 2},
            {"ideal_gas",  4, 4},    // 3 numeric + 1 string (solve target)
            {"normalize",  3, 3},
            {"pearson",    4, -1},   // variadic: even count, min 2 pairs
        };
    }

    CellValue call(const std::string& func_name,
                   const std::vector<CellValue>& args) const override;

private:
    std::string name_    = "Scientific";
    std::string version_ = "1.0.0";
    std::string type_    = "function";
};

}  // namespace magic::plugins
