#include "plugin/PluginManager.hpp"
#include <cstring>
#include <iostream>

namespace magic {

int count_args_from_signature(const char* sig) {
    const char* open = std::strchr(sig, '(');
    if (!open) return 0;
    const char* close = std::strchr(open, ')');
    if (!close) return 0;
    // Check for empty parens or "void()"
    if (close == open + 1) return 0;
    // Count commas + 1
    int count = 1;
    for (const char* p = open + 1; p < close; ++p) {
        if (*p == ',') ++count;
    }
    return count;
}

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

            auto inst = instance;
            std::string fn_name = fd.name;
            int min_args = fd.min_args;
            int max_args = fd.max_args;
            registry_.register_function(fd.name,
                [inst, fn_name, min_args, max_args](const std::vector<CellValue>& args) -> CellValue {
                    int n = static_cast<int>(args.size());
                    if (n < min_args) return CellValue{CellError::VALUE};
                    if (max_args >= 0 && n > max_args) return CellValue{CellError::VALUE};
                    return inst->call(fn_name, args);
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

            int nargs = count_args_from_signature(entry.signature);
            void* raw = lib->resolve<void*>(sym_name);
            auto captured_lib = lib;

            registry_.register_function(sym_name,
                [raw, nargs](const std::vector<CellValue>& args) -> CellValue {
                    int n = static_cast<int>(args.size());
                    if (n != nargs) return CellValue{CellError::VALUE};
                    switch (nargs) {
                    case 0: {
                        auto fn = reinterpret_cast<double(*)()>(raw);
                        return CellValue{fn()};
                    }
                    case 1: {
                        auto fn = reinterpret_cast<double(*)(double)>(raw);
                        return CellValue{fn(to_double(args[0]))};
                    }
                    case 2: {
                        auto fn = reinterpret_cast<double(*)(double, double)>(raw);
                        return CellValue{fn(to_double(args[0]), to_double(args[1]))};
                    }
                    case 3: {
                        auto fn = reinterpret_cast<double(*)(double, double, double)>(raw);
                        return CellValue{fn(to_double(args[0]), to_double(args[1]), to_double(args[2]))};
                    }
                    case 4: {
                        auto fn = reinterpret_cast<double(*)(double, double, double, double)>(raw);
                        return CellValue{fn(to_double(args[0]), to_double(args[1]), to_double(args[2]), to_double(args[3]))};
                    }
                    default:
                        return CellValue{CellError::VALUE};
                    }
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
