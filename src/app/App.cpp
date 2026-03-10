#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include "app/App.hpp"
#include "builtin/MathFunctions.hpp"
#include "builtin/LogicFunctions.hpp"
#include "builtin/StatFunctions.hpp"
#include "builtin/TextFunctions.hpp"
#include "builtin/SqlFunction.hpp"
#include "plugin/DropHandler.hpp"
#include "ui/StyleSetup.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <pybind11/embed.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace magic {

App::App() = default;

void App::run(int argc, char** argv) {
    pybind11::scoped_interpreter guard{};
    state_.settings.load();
    init_window();
    init_imgui();
    init_builtins();
    if (argc > 1 && argv && argv[1])
        (void)state_.open_file(argv[1]);
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

    auto& wr = state_.settings.window_rect;
    window_ = glfwCreateWindow(wr.w, wr.h, "CellWright", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    if (wr.x >= 0 && wr.y >= 0)
        glfwSetWindowPos(window_, wr.x, wr.y);

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

    // Intercept window close when there are unsaved changes
    glfwSetWindowUserPointer(window_, this);
    glfwSetWindowCloseCallback(window_, [](GLFWwindow* w) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app->state_.is_dirty()) {
            glfwSetWindowShouldClose(w, GLFW_FALSE);
            app->state_.main_window.request_close();
        }
    });
}

void App::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load Roboto as the default font (replaces ImGui's tiny ProggyClean)
    // Fall back to ImGui default if font file is missing
    const char* font_path = IMGUI_FONT_DIR "/Roboto-Medium.ttf";
    if (std::ifstream(font_path).good())
        io.Fonts->AddFontFromFileTTF(font_path, state_.settings.font_size);

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
    register_sql_function(state_.function_registry, state_.duckdb_engine,
                          state_.workbook);
}

void App::main_loop() {
    while (!glfwWindowShouldClose(window_) && !state_.main_window.should_quit()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Poll hot-reloadable plugins for changes
        if (int n = state_.plugin_manager.poll_reloads(); n > 0)
            state_.toasts.show("Plugin reloaded");

        // Rebuild font atlas if font size changed
        if (state_.font_rebuild_needed) {
            state_.font_rebuild_needed = false;
            ImGuiIO& font_io = ImGui::GetIO();
            font_io.Fonts->Clear();
            const char* fp = IMGUI_FONT_DIR "/Roboto-Medium.ttf";
            if (std::ifstream(fp).good())
                font_io.Fonts->AddFontFromFileTTF(fp, state_.settings.font_size);
            else
                font_io.Fonts->AddFontDefault();
            font_io.Fonts->Build();
        }

        state_.main_window.render(state_);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);

        // Update window title only when state changes
        {
            bool dirty = state_.is_dirty();
            if (state_.current_file != cached_title_file_ || dirty != cached_title_dirty_) {
                cached_title_file_ = state_.current_file;
                cached_title_dirty_ = dirty;
                cached_title_ = "CellWright";
                if (!state_.current_file.empty()) {
                    auto fname = std::filesystem::path(state_.current_file).filename().string();
                    cached_title_ += " — ";
                    if (dirty) cached_title_ += "[*]";
                    cached_title_ += fname;
                } else if (dirty) {
                    cached_title_ += " — [*]Untitled";
                }
                glfwSetWindowTitle(window_, cached_title_.c_str());
            }
        }

        // Sleep when idle to avoid burning CPU.
        // During interaction (mouse held, text input, drags, async work)
        // events keep waking us immediately so responsiveness is unchanged.
        ImGuiIO& io = ImGui::GetIO();
        bool active = io.MouseDown[0] || io.WantTextInput ||
                      state_.main_window.is_interaction_active() ||
                      state_.async_recalc.is_busy();
        if (!active)
            glfwWaitEventsTimeout(0.1);
    }
}

void App::shutdown() {
    // Save window position/size to settings
    if (window_) {
        auto& wr = state_.settings.window_rect;
        glfwGetWindowPos(window_, &wr.x, &wr.y);
        glfwGetWindowSize(window_, &wr.w, &wr.h);
    }
    state_.settings.save();

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
