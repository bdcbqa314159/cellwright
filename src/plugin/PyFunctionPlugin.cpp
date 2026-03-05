#include "plugin/PyFunctionPlugin.hpp"
#include <pybind11/stl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace magic {

PyFunctionPlugin::PyFunctionPlugin(const std::filesystem::path& py_path) {
    if (!std::filesystem::exists(py_path))
        throw std::runtime_error("Python plugin not found: " + py_path.string());

    name_ = py_path.stem().string();

    // Read the file contents
    std::ifstream ifs(py_path);
    if (!ifs)
        throw std::runtime_error("Cannot open: " + py_path.string());
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string code = ss.str();

    // Execute into an isolated dict (avoids sys.path pollution)
    scope_ = py::dict();
    scope_["__builtins__"] = py::module_::import("builtins").attr("__dict__");
    try {
        py::exec(code, scope_);
    } catch (const py::error_already_set& e) {
        throw std::runtime_error("Python syntax/exec error in " +
                                 py_path.string() + ": " + e.what());
    }

    // Parse the `functions` dict
    if (!scope_.contains("functions"))
        throw std::runtime_error("Python plugin missing `functions` dict: " +
                                 py_path.string());

    py::dict funcs = scope_["functions"].cast<py::dict>();
    for (auto& [key, val] : funcs) {
        std::string fn_name = key.cast<std::string>();

        // Verify the function is actually defined
        if (!scope_.contains(fn_name.c_str()))
            throw std::runtime_error("Python plugin declares '" + fn_name +
                                     "' in functions dict but no matching def found");

        py::dict desc = val.cast<py::dict>();
        int min_args = desc.contains("min_args") ? desc["min_args"].cast<int>() : 0;
        int max_args = desc.contains("max_args") ? desc["max_args"].cast<int>() : -1;

        descriptors_.push_back({fn_name, min_args, max_args});
    }
}

const std::string& PyFunctionPlugin::name() const { return name_; }
const std::string& PyFunctionPlugin::version() const { return version_; }
const std::string& PyFunctionPlugin::type() const { return type_; }

std::vector<FunctionDescriptor> PyFunctionPlugin::describe_functions() const {
    return descriptors_;
}

CellValue PyFunctionPlugin::call(const std::string& func_name,
                                  const std::vector<CellValue>& args) const {
    if (!scope_.contains(func_name.c_str()))
        return CellValue{CellError::NAME};

    try {
        py::object fn = scope_[func_name.c_str()];

        // Build argument tuple
        py::tuple py_args(args.size());
        for (size_t i = 0; i < args.size(); ++i)
            py_args[i] = to_python(args[i]);

        py::object result = fn(*py_args);
        return from_python(result);

    } catch (const py::error_already_set& e) {
        // Map Python exceptions to CellError
        std::string type_name = e.type() ?
            py::str(e.type().attr("__name__")).cast<std::string>() : "";

        if (type_name == "ZeroDivisionError") return CellValue{CellError::DIV0};
        if (type_name == "KeyError")          return CellValue{CellError::NAME};
        if (type_name == "ValueError")        return CellValue{CellError::VALUE};
        if (type_name == "TypeError")         return CellValue{CellError::VALUE};
        if (type_name == "IndexError")        return CellValue{CellError::REF};

        return CellValue{CellError::VALUE};
    }
}

py::object PyFunctionPlugin::to_python(const CellValue& v) {
    if (is_empty(v))  return py::none();
    if (is_number(v)) return py::float_(as_number(v));
    if (is_string(v)) return py::str(as_string(v));
    if (is_bool(v))   return py::bool_(std::get<bool>(v));
    if (is_error(v))  return py::none();  // errors become None in Python
    return py::none();
}

CellValue PyFunctionPlugin::from_python(const py::object& obj) {
    if (obj.is_none()) return CellValue{};

    // Check bool before int (bool is subclass of int in Python)
    if (py::isinstance<py::bool_>(obj))
        return CellValue{obj.cast<bool>()};
    if (py::isinstance<py::int_>(obj))
        return CellValue{obj.cast<double>()};
    if (py::isinstance<py::float_>(obj))
        return CellValue{obj.cast<double>()};
    if (py::isinstance<py::str>(obj))
        return CellValue{obj.cast<std::string>()};

    // Fallback: convert to string
    return CellValue{py::str(obj).cast<std::string>()};
}

}  // namespace magic
