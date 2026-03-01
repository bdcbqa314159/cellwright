#include "builtin/TextFunctions.hpp"
#include "formula/FunctionRegistry.hpp"
#include <algorithm>

namespace magic {

static std::string cell_to_string(const CellValue& v) {
    if (is_string(v)) return as_string(v);
    return to_display_string(v);
}

void register_text_functions(FunctionRegistry& reg) {
    reg.register_function("LEN", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        return CellValue{static_cast<double>(cell_to_string(args[0]).size())};
    });

    reg.register_function("LEFT", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        int n = args.size() > 1 ? static_cast<int>(to_double(args[1])) : 1;
        if (n < 0) return CellValue{CellError::VALUE};
        return CellValue{s.substr(0, n)};
    });

    reg.register_function("RIGHT", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        int n = args.size() > 1 ? static_cast<int>(to_double(args[1])) : 1;
        if (n < 0) return CellValue{CellError::VALUE};
        if (static_cast<size_t>(n) >= s.size()) return CellValue{s};
        return CellValue{s.substr(s.size() - n)};
    });

    reg.register_function("MID", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.size() < 3) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        int start = static_cast<int>(to_double(args[1])) - 1;  // 1-based
        int len = static_cast<int>(to_double(args[2]));
        if (start < 0 || len < 0) return CellValue{CellError::VALUE};
        return CellValue{s.substr(start, len)};
    });

    reg.register_function("UPPER", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return CellValue{s};
    });

    reg.register_function("LOWER", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return CellValue{s};
    });

    reg.register_function("TRIM", [](const std::vector<CellValue>& args) -> CellValue {
        if (args.empty()) return CellValue{CellError::VALUE};
        std::string s = cell_to_string(args[0]);
        // Remove leading/trailing spaces
        size_t start = s.find_first_not_of(' ');
        if (start == std::string::npos) return CellValue{std::string("")};
        size_t end = s.find_last_not_of(' ');
        // Collapse internal multiple spaces to single
        std::string result;
        bool prev_space = false;
        for (size_t i = start; i <= end; ++i) {
            if (s[i] == ' ') {
                if (!prev_space) result += ' ';
                prev_space = true;
            } else {
                result += s[i];
                prev_space = false;
            }
        }
        return CellValue{result};
    });

    reg.register_function("CONCATENATE", [](const std::vector<CellValue>& args) -> CellValue {
        std::string result;
        for (const auto& a : args)
            result += cell_to_string(a);
        return CellValue{result};
    });
}

}  // namespace magic
