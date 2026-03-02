#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include "app/App.hpp"
#include "builtin/MathFunctions.hpp"
#include "builtin/LogicFunctions.hpp"
#include "builtin/StatFunctions.hpp"
#include "builtin/TextFunctions.hpp"
#include "plugin/DropHandler.hpp"
#include "ui/StyleSetup.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <iostream>
#include <stdexcept>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace magic {

App::App() = default;

void App::run() {
    init_window();
    init_imgui();
    init_builtins();
    main_loop();
    shutdown();
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

void App::init_window() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    // macOS requires these hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(1400, 900, "Magic Dashboard", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // vsync

    // Install drop handler — trusted plugins load immediately, untrusted go to modal
    DropHandler::install(window_, [this](const std::string& path) {
        if (state_.plugin_manager.allowlist().is_trusted(path)) {
            state_.plugin_manager.load(path);
        } else {
            state_.pending_plugin_path = path;
        }
    });
}

void App::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    setup_style();
    apply_theme(Theme::Dark);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImPlot::CreateContext();
}

void App::init_builtins() {
    register_math_functions(state_.function_registry);
    register_logic_functions(state_.function_registry);
    register_stat_functions(state_.function_registry);
    register_text_functions(state_.function_registry);
}

void App::main_loop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        state_.main_window.render(state_);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);

        // Sleep when idle to avoid burning CPU.
        // During interaction (mouse held, text input, drags, async work)
        // events keep waking us immediately so responsiveness is unchanged.
        ImGuiIO& io = ImGui::GetIO();
        auto& gs = state_.main_window.grid_state();
        bool active = io.MouseDown[0] || io.WantTextInput ||
                      gs.drag_mode != CellDragMode::None ||
                      gs.formula_dragging ||
                      state_.async_recalc.is_busy();
        if (!active)
            glfwWaitEventsTimeout(0.1);
    }
}

void App::shutdown() {
    DropHandler::uninstall();
    state_.function_registry.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
}

}  // namespace magic
