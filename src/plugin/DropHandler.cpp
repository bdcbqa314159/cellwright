#include "plugin/DropHandler.hpp"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>

namespace magic {

DropHandler::LoadCallback DropHandler::callback_;

// Ordering requirement: uninstall() must be called before the objects captured
// by the callback are destroyed (i.e., before PluginManager is torn down).
void DropHandler::install(GLFWwindow* window, LoadCallback callback) {
    callback_ = std::move(callback);
    glfwSetDropCallback(window, glfw_drop_callback);
}

void DropHandler::uninstall() {
    callback_ = nullptr;
}

void DropHandler::glfw_drop_callback(GLFWwindow*, int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        std::string path = paths[i];
        if (is_plugin_file(path)) {
            std::cout << "[DropHandler] Dropped: " << path << "\n";
            if (callback_) callback_(path);
        } else {
            std::cout << "[DropHandler] Ignoring non-library file: " << path << "\n";
        }
    }
}

bool DropHandler::is_plugin_file(const std::string& path) {
    std::filesystem::path p(path);
    auto ext = p.extension().string();
    return ext == ".dylib" || ext == ".so" || ext == ".dll" || ext == ".py";
}

}  // namespace magic
