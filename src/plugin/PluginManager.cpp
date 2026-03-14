#include "plugin/PluginManager.hpp"
#include "core/Workbook.hpp"
#include "util/Sha256.hpp"
#include <cstring>
#include <filesystem>
#include <iostream>

namespace magic {

int count_args_from_signature(const char* sig) {
    if (!sig) return 0;
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

// ── IFunction bridge helper ──────────────────────────────────────────────────
// Registers a plugin function with arity checks, avoiding lambda duplication.

template <typename Callable>
static void register_ifunc_bridge(FunctionRegistry& registry, const std::string& name,
                                   int min_args, int max_args, Callable&& callable) {
    registry.register_function(name,
        [callable = std::forward<Callable>(callable), min_args, max_args]
        (const std::vector<CellValue>& args) -> CellValue {
            int n = static_cast<int>(args.size());
            if (n < min_args) return CellValue{CellError::VALUE};
            if (max_args >= 0 && n > max_args) return CellValue{CellError::VALUE};
            return callable(args);
        });
}

// ── Constructors ────────────────────────────────────────────────────────────

PluginManager::PluginManager(FunctionRegistry& registry)
    : registry_(registry) {}

PluginManager::PluginManager(FunctionRegistry& registry, std::filesystem::path allowlist_path)
    : registry_(registry), allowlist_(std::move(allowlist_path)) {}

// ── Security gate ───────────────────────────────────────────────────────────

bool PluginManager::check_trust(const std::string& path) {
    if (allowlist_.is_trusted(path)) return true;

    std::string hash = sha256_of_file(path);
    std::cerr << "[PluginManager] UNTRUSTED plugin blocked: " << path << "\n"
              << "  SHA-256: " << hash << "\n"
              << "  To trust: call trust_and_load(\"" << path << "\")\n";
    return false;
}

// ── Probe ───────────────────────────────────────────────────────────────────

PluginKind PluginManager::probe_kind(plugin_arch::DynamicLibrary& lib) {
    if (lib.has("plugin_describe")) return PluginKind::CABI;
    if (lib.has("allocator"))       return PluginKind::IFunction;  // tentative
    return PluginKind::Unknown;
}

// ── Python detection ────────────────────────────────────────────────────────

bool PluginManager::is_python_plugin(const std::string& path) {
    return std::filesystem::path(path).extension() == ".py";
}

// ── Main load pipeline ──────────────────────────────────────────────────────

bool PluginManager::load(const std::string& path) {
    // Phase 1: security gate (0 dlopen if untrusted)
    if (!check_trust(path)) return false;

    // Python plugins: skip dlopen, go straight to Python loader
    if (is_python_plugin(path)) return load_python(path);

    // Phase 2: single dlopen to probe plugin kind
    std::shared_ptr<plugin_arch::DynamicLibrary> probe;
    try {
        probe = std::make_shared<plugin_arch::DynamicLibrary>(path);
    } catch (const std::exception& e) {
        std::cerr << "[PluginManager] Failed to open: " << path << " — " << e.what() << "\n";
        return false;
    }

    // Phase 3: dispatch based on probe
    PluginKind kind = probe_kind(*probe);

    switch (kind) {
    case PluginKind::CABI:
        // Reuse the probe handle directly — total: 1 dlopen
        return load_cabi(std::move(probe));

    case PluginKind::IFunction:
        // PluginLoader<T> needs its own dlopen, so release probe first
        probe.reset();
        if (load_ifunction(path)) return true;
        // Fallback: maybe it's an IPanel with an allocator symbol
        return load_ipanel(path);

    case PluginKind::IPanel:
    case PluginKind::Unknown:
        std::cerr << "[PluginManager] Unknown plugin type: " << path << "\n";
        return false;
    }
    return false;
}

bool PluginManager::trust_and_load(const std::string& path) {
    allowlist_.trust(path);
    return load(path);
}

// ── IFunction loader ────────────────────────────────────────────────────────

bool PluginManager::load_ifunction(const std::string& path) {
    try {
        auto hot = std::make_shared<plugin_arch::HotPluginLoader<IFunction>>(path);
        auto instance = hot->get_instance();

        LoadedPlugin lp;
        lp.path = path;
        lp.name = instance->name();

        HotPluginEntry entry;
        entry.loader = hot;

        for (const auto& fd : instance->describe_functions()) {
            lp.function_names.push_back(fd.name);
            entry.function_names.push_back(fd.name);

            auto hot_ref = hot;
            std::string fn_name = fd.name;
            register_ifunc_bridge(registry_, fd.name, fd.min_args, fd.max_args,
                [hot_ref, fn_name](const std::vector<CellValue>& args) {
                    return hot_ref->get_instance()->call(fn_name, args);
                });
        }

        hot_loaders_.push_back(std::move(entry));
        loaded_.push_back(std::move(lp));
        std::cout << "[PluginManager] Loaded C++ plugin: " << loaded_.back().name << "\n";
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// ── IPanel loader ───────────────────────────────────────────────────────────

bool PluginManager::load_ipanel(const std::string& path) {
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

void PluginManager::init_panels(Workbook& workbook) {
    for (auto& pi : panels_) {
        if (!pi.initialized) {
            pi.panel->init(workbook);
            pi.initialized = true;
        }
    }
}

void PluginManager::render_panels() {
    for (auto& pi : panels_) {
        pi.panel->render();
    }
}

int PluginManager::poll_reloads() {
    int count = 0;
    for (auto& entry : hot_loaders_) {
        if (entry.loader->check_and_reload()) {
            // Unregister old functions
            for (const auto& name : entry.function_names)
                (void)registry_.unregister_function(name);

            // Re-describe functions from the new instance
            auto instance = entry.loader->get_instance();
            entry.function_names.clear();

            for (const auto& fd : instance->describe_functions()) {
                entry.function_names.push_back(fd.name);

                auto hot_ref = entry.loader;
                std::string fn_name = fd.name;
                register_ifunc_bridge(registry_, fd.name, fd.min_args, fd.max_args,
                    [hot_ref, fn_name](const std::vector<CellValue>& args) {
                        return hot_ref->get_instance()->call(fn_name, args);
                    });
            }

            // Update loaded_ metadata to stay in sync
            std::string lpath = entry.loader->library_path();
            for (auto& lp : loaded_) {
                if (lp.path == lpath) {
                    lp.function_names = entry.function_names;
                    break;
                }
            }

            std::cout << "[PluginManager] Hot-reloaded: " << lpath << "\n";
            ++count;
        }
    }
    return count;
}

// ── C ABI loader (reuses probe handle) ──────────────────────────────────────

bool PluginManager::load_cabi(std::shared_ptr<plugin_arch::DynamicLibrary> lib) {
    try {
        auto describe_fn = lib->resolve<const plugin_arch::PluginDescriptor* (*)()>("plugin_describe");
        const auto* desc = describe_fn();
        if (!desc) return false;

        LoadedPlugin lp;
        lp.path = lib->path();
        lp.name = desc->name;

        for (int i = 0; i < desc->function_count; ++i) {
            const auto& entry = desc->functions[i];
            std::string sym_name = entry.name;
            lp.function_names.push_back(sym_name);

            int nargs = count_args_from_signature(entry.signature);
            auto raw = reinterpret_cast<void*>(lib->resolve<void(*)()>(sym_name));
            auto captured_lib = lib;

            registry_.register_function(sym_name,
                [raw, nargs, captured_lib](const std::vector<CellValue>& args) -> CellValue {
                    int n = static_cast<int>(args.size());
                    if (n != nargs) return CellValue{CellError::VALUE};
                    // Arity-based dispatch limited to 0–4 args. For higher arities,
            // libffi or a generic calling convention would be needed.
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
    } catch (const std::exception&) {
        return false;
    }
}

// ── Python plugin loader ────────────────────────────────────────────────────

bool PluginManager::load_python(const std::string& path) {
    try {
        auto instance = std::make_shared<PyFunctionPlugin>(std::filesystem::path(path));

        LoadedPlugin lp;
        lp.path = path;
        lp.name = instance->name();

        for (const auto& fd : instance->describe_functions()) {
            lp.function_names.push_back(fd.name);

            auto inst = instance;
            std::string fn_name = fd.name;
            register_ifunc_bridge(registry_, fd.name, fd.min_args, fd.max_args,
                [inst, fn_name](const std::vector<CellValue>& args) {
                    return inst->call(fn_name, args);
                });
        }

        py_plugins_.push_back(std::move(instance));
        loaded_.push_back(std::move(lp));
        std::cout << "[PluginManager] Loaded Python plugin: " << loaded_.back().name << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PluginManager] Failed to load Python plugin: " << path
                  << " — " << e.what() << "\n";
        return false;
    }
}

}  // namespace magic
