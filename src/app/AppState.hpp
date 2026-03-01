#pragma once
#include "core/Workbook.hpp"
#include "formula/FunctionRegistry.hpp"
#include "plugin/PluginManager.hpp"
#include "ui/MainWindow.hpp"

namespace magic {

struct AppState {
    Workbook workbook;
    FunctionRegistry function_registry;
    PluginManager plugin_manager{function_registry};
    MainWindow main_window;
};

}  // namespace magic
