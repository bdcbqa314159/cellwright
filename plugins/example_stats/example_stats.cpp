#include "example_stats.hpp"
#include <PluginFactory.hpp>

namespace magic::plugins {

CellValue ExampleStats::call(const std::string& func_name, const std::vector<CellValue>& args) const {
    if (func_name == "ZSCORE") {
        double x = to_double(args[0]);
        double mean = to_double(args[1]);
        double stddev = to_double(args[2]);
        if (stddev == 0.0) return CellValue{CellError::DIV0};
        return CellValue{(x - mean) / stddev};
    }
    return CellValue{CellError::NAME};
}

}  // namespace magic::plugins

REGISTER_PLUGIN(magic::plugins::ExampleStats)
