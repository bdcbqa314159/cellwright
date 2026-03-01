#include "builtin/StatFunctions.hpp"
#include "formula/FunctionRegistry.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace magic {

void register_stat_functions(FunctionRegistry& reg) {
    reg.register_function("STDEV", [](const std::vector<CellValue>& args) -> CellValue {
        std::vector<double> nums;
        for (const auto& a : args)
            if (is_number(a)) nums.push_back(as_number(a));
        if (nums.size() < 2) return CellValue{CellError::DIV0};

        double mean = std::accumulate(nums.begin(), nums.end(), 0.0) / nums.size();
        double sq_sum = 0.0;
        for (double x : nums) sq_sum += (x - mean) * (x - mean);
        return CellValue{std::sqrt(sq_sum / (nums.size() - 1))};
    });

    reg.register_function("MEDIAN", [](const std::vector<CellValue>& args) -> CellValue {
        std::vector<double> nums;
        for (const auto& a : args)
            if (is_number(a)) nums.push_back(as_number(a));
        if (nums.empty()) return CellValue{CellError::VALUE};

        std::sort(nums.begin(), nums.end());
        size_t n = nums.size();
        if (n % 2 == 1) return CellValue{nums[n / 2]};
        return CellValue{(nums[n / 2 - 1] + nums[n / 2]) / 2.0};
    });

    reg.register_function("PERCENTILE", [](const std::vector<CellValue>& args) -> CellValue {
        // Last arg is the percentile (0-1), rest are data
        if (args.size() < 2) return CellValue{CellError::VALUE};

        double k = to_double(args.back());
        if (k < 0.0 || k > 1.0) return CellValue{CellError::VALUE};

        std::vector<double> nums;
        for (size_t i = 0; i + 1 < args.size(); ++i)
            if (is_number(args[i])) nums.push_back(as_number(args[i]));
        if (nums.empty()) return CellValue{CellError::VALUE};

        std::sort(nums.begin(), nums.end());
        double idx = k * (nums.size() - 1);
        size_t lo = static_cast<size_t>(std::floor(idx));
        size_t hi = static_cast<size_t>(std::ceil(idx));
        if (lo == hi) return CellValue{nums[lo]};
        double frac = idx - lo;
        return CellValue{nums[lo] * (1 - frac) + nums[hi] * frac};
    });

    reg.register_function("CORREL", [](const std::vector<CellValue>& args) -> CellValue {
        // Expects an even number of args: first half = X, second half = Y
        if (args.size() < 4 || args.size() % 2 != 0) return CellValue{CellError::VALUE};

        size_t n = args.size() / 2;
        std::vector<double> x, y;
        for (size_t i = 0; i < n; ++i) {
            x.push_back(to_double(args[i]));
            y.push_back(to_double(args[n + i]));
        }

        double mx = std::accumulate(x.begin(), x.end(), 0.0) / n;
        double my = std::accumulate(y.begin(), y.end(), 0.0) / n;

        double num = 0, dx2 = 0, dy2 = 0;
        for (size_t i = 0; i < n; ++i) {
            double dx = x[i] - mx, dy = y[i] - my;
            num += dx * dy;
            dx2 += dx * dx;
            dy2 += dy * dy;
        }
        double denom = std::sqrt(dx2 * dy2);
        if (denom == 0) return CellValue{CellError::DIV0};
        return CellValue{num / denom};
    });

    reg.register_function("VLOOKUP", [](const std::vector<CellValue>& args) -> CellValue {
        // VLOOKUP(lookup_value, table_range..., col_index)
        // Simplified: expects flat args where last arg is col index
        // In practice, table_range gets expanded by evaluator
        if (args.size() < 3) return CellValue{CellError::VALUE};
        // Minimal implementation — exact match in first "column"
        return CellValue{CellError::NA};  // Phase 2 placeholder
    });
}

}  // namespace magic
