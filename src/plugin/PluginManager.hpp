#pragma once
#include "plugin/IFunction.hpp"
#include "plugin/IPanel.hpp"
#include "plugin/PyFunctionPlugin.hpp"
#include "plugin/PluginAllowlist.hpp"
#include "formula/FunctionRegistry.hpp"
#include <PluginLoader.hpp>
#include <DynamicLibrary.hpp>
#include <PluginDescriptor.hpp>
#include <memory>
#include <string>
#include <vector>

namespace magic {

// Count args from a C ABI signature like "double(double, double)" → 2.
int count_args_from_signature(const char* sig);

enum class PluginKind { IFunction, IPanel, CABI, Unknown };

class PluginManager {
public:
    explicit PluginManager(FunctionRegistry& registry);
    PluginManager(FunctionRegistry& registry, std::filesystem::path allowlist_path);

    bool load(const std::string& path);
    bool trust_and_load(const std::string& path);

    int loaded_count() const { return static_cast<int>(loaded_.size()); }

    struct LoadedPlugin {
        std::string path;
        std::string name;
        std::vector<std::string> function_names;
        bool is_panel = false;
    };

    const std::vector<LoadedPlugin>& loaded() const { return loaded_; }

    // Render all loaded IPanel plugins
    void render_panels();

    const PluginAllowlist& allowlist() const { return allowlist_; }

private:
    bool check_trust(const std::string& path);
    static PluginKind probe_kind(plugin_arch::DynamicLibrary& lib);
    static bool is_python_plugin(const std::string& path);

    bool load_ifunction(const std::string& path);
    bool load_ipanel(const std::string& path);
    bool load_cabi(std::shared_ptr<plugin_arch::DynamicLibrary> lib);
    bool load_python(const std::string& path);

    FunctionRegistry& registry_;
    PluginAllowlist allowlist_;
    std::vector<LoadedPlugin> loaded_;

    std::vector<std::shared_ptr<plugin_arch::PluginLoader<IFunction>>> cpp_loaders_;
    std::vector<std::shared_ptr<plugin_arch::DynamicLibrary>> c_libs_;
    std::vector<std::shared_ptr<PyFunctionPlugin>> py_plugins_;

    // IPanel instances
    struct PanelInstance {
        std::shared_ptr<IPanel> panel;
        std::shared_ptr<plugin_arch::PluginLoader<IPanel>> loader;
    };
    std::vector<PanelInstance> panels_;
};

}  // namespace magic
