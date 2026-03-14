#include "builtin/MathFunctions.hpp"
#include "formula/FunctionRegistry.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace magic {

void register_math_functions(FunctionRegistry& reg) {
    reg.register_function("SUM", [](const std::vector<CellValue>& args) -> CellValue {
        double sum = 0.0;
        for (const auto& a : args) {
            if (is_number(a)) sum += as_number(a);
            else if (is_bool(a)) sum += std::get<bool>(a) ? 1.0 : 0.0;
            // skip empty, strings
        }
        return CellValue{sum};
    }, "SUM(number1, [number2], ...)");

    reg.register_function("AVERAGE", [](const std::vector<CellValue>& args) -> CellValue {
        double sum = 0.0;
        int count = 0;
        for (const auto& a : args) {
            if (is_number(a)) { sum += as_number(a); ++count; }
            else if (is_bool(a)) { sum += std::get<bool>(a) ? 1.0 : 0.0; ++count; }
        }
        if (count == 0) return CellValue{CellError::DIV0};
        return CellValue{sum / count};
    }, "AVERAGE(number1, [number2], ...)");

    reg.register_function("MIN", [](const std::vector<CellValue>& args) -> CellValue {
        double result = std::numeric_limits<double>::infinity();
        bool found = false;
        for (const auto& a : args) {
            if (is_number(a)) { result = std::min(result, as_number(a)); found = true; }
        }
        return found ? CellValue{result} : CellValue{0.0};
    }, "MIN(number1, [number2], ...)");

    reg.register_function("MAX", [](const std::vector<CellValue>& args) -> CellValue {
        double result = -std::numeric_limits<double>::infinity();
        bool found = false;
        for (const auto& a : args) {
            if (is_number(a)) { result = std::max(result, as_number(a)); found = true; }
        }
        return found ? CellValue{result} : CellValue{0.0};
    }, "MAX(number1, [number2], ...)");

    reg.register_function("COUNT", [](const std::vector<CellValue>& args) -> CellValue {
        int count = 0;
        for (const auto& a : args) {
            if (is_number(a)) ++count;
        }
        return CellValue{static_cast<double>(count)};
    }, "COUNT(value1, [value2], ...)");

    reg.register_function("ABS", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        return CellValue{std::abs(to_double(args[0]))};
    }, "ABS(number)");

    reg.register_function("ROUND", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        double val = to_double(args[0]);
        int digits = args.size() > 1 ? static_cast<int>(to_double(args[1])) : 0;
        double factor = std::pow(10.0, digits);
        return CellValue{std::round(val * factor) / factor};
    }, "ROUND(number, [digits])");
}

}  // namespace magic
