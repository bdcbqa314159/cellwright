#pragma once
#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;

namespace magic {

class DropHandler {
public:
    using LoadCallback = std::function<void(const std::string& path)>;

    static void install(GLFWwindow* window, LoadCallback callback);
    static void uninstall();

private:
    static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths);
    static bool is_plugin_file(const std::string& path);
    static LoadCallback callback_;
};

}  // namespace magic
