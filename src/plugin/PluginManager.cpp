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

// ── Constructors ────────────────────────────────────────────────────────────

PluginManager::PluginManager(FunctionRegistry& registry)
    : registry_(registry) {}

PluginManager::PluginManager(FunctionRegistry& registry, std::filesystem::path allowlist_path)
    : registry_(registry), allowlist_(std::move(allowlist_path)) {}

// ── Security gate ───────────────────────────────────────────────────────────

bool PluginManager::check_trust(const std::string& path) {
    if (allowlist_.is_trusted(path)) return true;

    std::string hash = PluginAllowlist::sha256_of_file(path);
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

// ── Main load pipeline ──────────────────────────────────────────────────────

bool PluginManager::load(const std::string& path) {
    // Phase 1: security gate (0 dlopen if untrusted)
    if (!check_trust(path)) return false;

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

void PluginManager::render_panels() {
    for (auto& pi : panels_) {
        pi.panel->render();
    }
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
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace magic
