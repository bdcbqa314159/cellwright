#pragma once
#include "plugin/IFunction.hpp"
#include "formula/FunctionRegistry.hpp"
#include <PluginLoader.hpp>
#include <DynamicLibrary.hpp>
#include <PluginDescriptor.hpp>
#include <memory>
#include <string>
#include <vector>

namespace magic {

class PluginManager {
public:
    explicit PluginManager(FunctionRegistry& registry);

    // Load a plugin from a shared library path.
    // Tries IFunction (C++ interface) first, then falls back to C ABI (PluginDescriptor).
    bool load(const std::string& path);

    int loaded_count() const { return static_cast<int>(loaded_.size()); }

    struct LoadedPlugin {
        std::string path;
        std::string name;
        std::vector<std::string> function_names;
    };

    const std::vector<LoadedPlugin>& loaded() const { return loaded_; }

private:
    bool try_load_ifunction(const std::string& path);
    bool try_load_cabi(const std::string& path);

    FunctionRegistry& registry_;
    std::vector<LoadedPlugin> loaded_;

    // Keep loaders alive so shared libs stay loaded
    std::vector<std::shared_ptr<plugin_arch::PluginLoader<IFunction>>> cpp_loaders_;
    std::vector<std::shared_ptr<plugin_arch::DynamicLibrary>> c_libs_;
};

}  // namespace magic
