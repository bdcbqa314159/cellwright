#include "example_stats.hpp"
#include <PluginFactory.hpp>
#include <cmath>
#include <stdexcept>

namespace magic::plugins {

double ExampleStats::call(const std::string& func_name, const std::vector<double>& args) const {
    if (func_name == "ZSCORE") {
        if (args.size() < 3) return std::nan("");
        double x = args[0];
        double mean = args[1];
        double stddev = args[2];
        if (stddev == 0.0) return std::nan("");
        return (x - mean) / stddev;
    }
    return std::nan("");
}

}  // namespace magic::plugins

REGISTER_PLUGIN(magic::plugins::ExampleStats)
