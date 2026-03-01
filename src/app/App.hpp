#pragma once
#include "app/AppState.hpp"

struct GLFWwindow;

namespace magic {

class App {
public:
    App();
    void run();

private:
    void init_window();
    void init_imgui();
    void init_builtins();
    void main_loop();
    void shutdown();

    GLFWwindow* window_ = nullptr;
    AppState state_;
};

}  // namespace magic
