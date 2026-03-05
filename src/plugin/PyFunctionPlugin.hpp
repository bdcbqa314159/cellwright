#pragma once
#include "plugin/IFunction.hpp"
#include <pybind11/embed.h>
#include <filesystem>
#include <string>
#include <vector>

namespace py = pybind11;

namespace magic {

// Adapter: loads a .py file and exposes its functions through IFunction.
// The .py file must define a `functions` dict and matching def's.
class PyFunctionPlugin : public IFunction {
public:
    // Throws std::runtime_error if the .py file is invalid.
    explicit PyFunctionPlugin(const std::filesystem::path& py_path);

    const std::string& name() const override;
    const std::string& version() const override;
    const std::string& type() const override;

    std::vector<FunctionDescriptor> describe_functions() const override;
    CellValue call(const std::string& func_name,
                   const std::vector<CellValue>& args) const override;

    // Convert between CellValue and Python objects
    static py::object to_python(const CellValue& v);
    static CellValue from_python(const py::object& obj);

private:
    std::string name_;
    std::string version_{"1.0.0"};
    std::string type_{"function"};
    std::vector<FunctionDescriptor> descriptors_;
    py::dict scope_;
};

}  // namespace magic
