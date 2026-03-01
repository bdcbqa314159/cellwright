#include "plugin/PluginManager.hpp"
#include <iostream>

namespace magic {

PluginManager::PluginManager(FunctionRegistry& registry) : registry_(registry) {}

bool PluginManager::load(const std::string& path) {
    if (try_load_ifunction(path)) return true;
    if (try_load_ipanel(path)) return true;
    if (try_load_cabi(path)) return true;
    std::cerr << "[PluginManager] Failed to load: " << path << "\n";
    return false;
}

bool PluginManager::try_load_ipanel(const std::string& path) {
    try {
        auto loader = std::make_shared<plugin_arch::PluginLoader<IPanel>>(path);
        auto instance = loader->get_instance();

        LoadedPlugin lp;
        lp.path = path;
        lp.name = instance->name();
        lp.is_panel = true;

        panels_.push_back({instance, loader});
        loaded_.push_back(std::move(lp));
        std::cout << "[PluginManager] Loaded panel plugin: " << loaded_.back().name << "\n";
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void PluginManager::render_panels() {
    for (auto& pi : panels_) {
        pi.panel->render();
    }
}

bool PluginManager::try_load_ifunction(const std::string& path) {
    try {
        auto loader = std::make_shared<plugin_arch::PluginLoader<IFunction>>(path);
        auto instance = loader->get_instance();

        LoadedPlugin lp;
        lp.path = path;
        lp.name = instance->name();

        for (const auto& fd : instance->describe_functions()) {
            lp.function_names.push_back(fd.name);

            // Capture shared_ptr to keep plugin alive
            auto inst = instance;
            std::string fn_name = fd.name;
            registry_.register_function(fd.name,
                [inst, fn_name](const std::vector<CellValue>& args) -> CellValue {
                    std::vector<double> dargs;
                    dargs.reserve(args.size());
                    for (const auto& a : args) dargs.push_back(to_double(a));
                    return CellValue{inst->call(fn_name, dargs)};
                });
        }

        cpp_loaders_.push_back(std::move(loader));
        loaded_.push_back(std::move(lp));
        std::cout << "[PluginManager] Loaded C++ plugin: " << loaded_.back().name << "\n";
        return true;
    } catch (const std::exception& e) {
        // Not a valid IFunction plugin, that's fine
        return false;
    }
}

bool PluginManager::try_load_cabi(const std::string& path) {
    try {
        auto lib = std::make_shared<plugin_arch::DynamicLibrary>(path);

        if (!lib->has("plugin_describe")) return false;

        auto describe_fn = lib->resolve<const plugin_arch::PluginDescriptor* (*)()>("plugin_describe");
        const auto* desc = describe_fn();
        if (!desc) return false;

        LoadedPlugin lp;
        lp.path = path;
        lp.name = desc->name;

        for (int i = 0; i < desc->function_count; ++i) {
            const auto& entry = desc->functions[i];
            std::string sym_name = entry.name;
            lp.function_names.push_back(sym_name);

            // Resolve the function symbol
            auto fn_ptr = lib->resolve<double (*)(double, double)>(sym_name);

            // Register as a spreadsheet function (assumes double(double, double) for now)
            auto captured_lib = lib;
            registry_.register_function(sym_name,
                [fn_ptr](const std::vector<CellValue>& args) -> CellValue {
                    if (args.size() < 2) {
                        // Try as single-arg
                        if (args.size() == 1) {
                            // Cast to single-arg variant
                            auto single_fn = reinterpret_cast<double (*)(double)>(reinterpret_cast<void*>(fn_ptr));
                            return CellValue{single_fn(to_double(args[0]))};
                        }
                        return CellValue{CellError::VALUE};
                    }
                    return CellValue{fn_ptr(to_double(args[0]), to_double(args[1]))};
                });
        }

        c_libs_.push_back(std::move(lib));
        loaded_.push_back(std::move(lp));
        std::cout << "[PluginManager] Loaded C ABI plugin: " << loaded_.back().name << "\n";
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

}  // namespace magic
