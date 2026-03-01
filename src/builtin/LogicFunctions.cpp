#include "builtin/LogicFunctions.hpp"
#include "formula/FunctionRegistry.hpp"

namespace magic {

void register_logic_functions(FunctionRegistry& reg) {
    reg.register_function("IF", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.size() < 2) return CellValue{CellError::VALUE};
        bool condition = to_double(args[0]) != 0.0;
        if (condition) return args[1];
        return args.size() > 2 ? args[2] : CellValue{false};
    });

    reg.register_function("AND", [](const std::vector<CellValue>& args) -> CellValue {
        for (const auto& a : args) {
            if (to_double(a) == 0.0) return CellValue{false};
        }
        return CellValue{true};
    });

    reg.register_function("OR", [](const std::vector<CellValue>& args) -> CellValue {
        for (const auto& a : args) {
            if (to_double(a) != 0.0) return CellValue{true};
        }
        return CellValue{false};
    });

    reg.register_function("NOT", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        return CellValue{to_double(args[0]) == 0.0};
    });
}

}  // namespace magic
