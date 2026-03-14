#pragma once
#include "app/AppState.hpp"
#include <string>

struct GLFWwindow;

namespace magic {

class App {
public:
    App();
    void run(int argc = 0, char** argv = nullptr);

private:
    void init_window();
    void init_imgui();
    void init_builtins();
    void main_loop();
    void shutdown();

    void rebuild_fonts();

    GLFWwindow* window_ = nullptr;
    AppState state_;
    float dpi_scale_ = 1.0f;

    // Cached title state to avoid rebuilding every frame
    std::string cached_title_;
    std::string cached_title_file_;
    bool cached_title_dirty_ = false;
};

}  // namespace magic
