#include "ui/MainWindow.hpp"
#include "app/AppState.hpp"
#include "core/Workbook.hpp"
#include "core/Clipboard.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "core/DateSerial.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "plugin/PluginAllowlist.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Formula helpers ─────────────────────────────────────────────────────────

static void collect_refs(const ASTNode& node, std::vector<CellAddress>& refs) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CellRefNode>) {
            refs.push_back(v.addr);
        } else if constexpr (std::is_same_v<T, RangeNode>) {
            int32_t c1 = std::min(v.from.col, v.to.col);
            int32_t c2 = std::max(v.from.col, v.to.col);
            int32_t r1 = std::min(v.from.row, v.to.row);
            int32_t r2 = std::max(v.from.row, v.to.row);
            for (int32_t c = c1; c <= c2; ++c)
                for (int32_t r = r1; r <= r2; ++r)
                    refs.push_back({c, r});
        } else if constexpr (std::is_same_v<T, BinOpNode>) {
            collect_refs(*v.left, refs);
            collect_refs(*v.right, refs);
        } else if constexpr (std::is_same_v<T, UnaryOpNode>) {
            collect_refs(*v.operand, refs);
        } else if constexpr (std::is_same_v<T, FuncCallNode>) {
            for (const auto& arg : v.args) collect_refs(*arg, refs);
        } else if constexpr (std::is_same_v<T, CompareNode>) {
            collect_refs(*v.left, refs);
            collect_refs(*v.right, refs);
        }
    }, node.value);
}

static void recalc_dependents(Sheet& sheet, const CellAddress& addr,
                               DependencyGraph& dep_graph,
                               const FunctionRegistry& registry,
                               Workbook* workbook = nullptr) {
    std::unordered_set<CellAddress> changed = {addr};
    auto order = dep_graph.recalc_order(changed);
    Evaluator eval(sheet, registry, workbook);
    for (const auto& dep_cell : order) {
        if (dep_cell == addr) continue;
        if (!sheet.has_formula(dep_cell)) continue;
        try {
            auto dep_tokens = Tokenizer::tokenize(sheet.get_formula(dep_cell));
            auto dep_formula = Parser::parse(dep_tokens);
            sheet.set_value(dep_cell, eval.evaluate(*dep_formula.root));
        } catch (...) {
            sheet.set_value(dep_cell, CellValue{CellError::VALUE});
        }
    }
}

static void process_cell_input(const char* buf, Sheet& sheet, const CellAddress& addr,
                                AppState& state) {
    std::string input(buf);
    CellValue old_val = sheet.get_value(addr);
    std::string old_formula = sheet.get_formula(addr);

    if (input.empty()) {
        auto cmd = std::make_unique<SetValueCommand>(addr, CellValue{}, old_val, old_formula);
        state.active_state().undo_manager.execute(std::move(cmd), sheet);
        state.active_state().dep_graph.set_dependencies(addr, {});
        recalc_dependents(sheet, addr, state.active_state().dep_graph, state.function_registry, &state.workbook);
        return;
    }

    if (input[0] == '=') {
        std::string formula = input.substr(1);
        try {
            auto tokens = Tokenizer::tokenize(formula);
            auto parsed = Parser::parse(tokens);

            std::vector<CellAddress> refs;
            collect_refs(*parsed.root, refs);
            state.active_state().dep_graph.set_dependencies(addr, refs);

            Evaluator eval(sheet, state.function_registry, &state.workbook);
            CellValue result = eval.evaluate(*parsed.root);

            auto cmd = std::make_unique<SetFormulaCommand>(addr, formula, result, old_val, old_formula);
            state.active_state().undo_manager.execute(std::move(cmd), sheet);

            recalc_dependents(sheet, addr, state.active_state().dep_graph, state.function_registry, &state.workbook);
        } catch (const std::exception&) {
            state.active_state().dep_graph.set_dependencies(addr, {});  // clear stale deps (#29)
            auto cmd = std::make_unique<SetFormulaCommand>(
                addr, formula, CellValue{CellError::VALUE}, old_val, old_formula);
            state.active_state().undo_manager.execute(std::move(cmd), sheet);
        }
    } else {
        state.active_state().dep_graph.set_dependencies(addr, {});
        CellValue new_val;

        // Try date parsing first (prevents "12/12/2024" → 12.0)
        auto date_result = parse_date(input);
        if (date_result) {
            new_val = CellValue{date_result->serial};
            // Auto-apply DATE format if cell is currently GENERAL
            CellFormat cur = state.active_state().format_map.get(addr);
            if (cur.type == FormatType::GENERAL) {
                CellFormat date_fmt;
                date_fmt.type = FormatType::DATE;
                date_fmt.date_input_hint = date_result->input_hint;
                state.active_state().format_map.set(addr, date_fmt);
            }
        } else {
            try {
                size_t pos = 0;
                double d = std::stod(input, &pos);
                if (pos == input.size())
                    new_val = CellValue{d};
                else
                    new_val = CellValue{input};
            } catch (...) {
                new_val = CellValue{input};
            }
        }

        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, old_val, old_formula);
        state.active_state().undo_manager.execute(std::move(cmd), sheet);
        recalc_dependents(sheet, addr, state.active_state().dep_graph, state.function_registry, &state.workbook);
    }
}

// Like process_cell_input but without recalc_dependents calls (for batch use).
static void set_cell_no_recalc(const char* buf, Sheet& sheet, const CellAddress& addr,
                                AppState& state) {
    std::string input(buf);
    CellValue old_val = sheet.get_value(addr);
    std::string old_formula = sheet.get_formula(addr);

    if (input.empty()) {
        auto cmd = std::make_unique<SetValueCommand>(addr, CellValue{}, old_val, old_formula);
        state.active_state().undo_manager.execute(std::move(cmd), sheet);
        state.active_state().dep_graph.set_dependencies(addr, {});
        return;
    }

    if (input[0] == '=') {
        std::string formula = input.substr(1);
        try {
            auto tokens = Tokenizer::tokenize(formula);
            auto parsed = Parser::parse(tokens);

            std::vector<CellAddress> refs;
            collect_refs(*parsed.root, refs);
            state.active_state().dep_graph.set_dependencies(addr, refs);

            Evaluator eval(sheet, state.function_registry, &state.workbook);
            CellValue result = eval.evaluate(*parsed.root);

            auto cmd = std::make_unique<SetFormulaCommand>(addr, formula, result, old_val, old_formula);
            state.active_state().undo_manager.execute(std::move(cmd), sheet);
        } catch (const std::exception&) {
            state.active_state().dep_graph.set_dependencies(addr, {});  // clear stale deps (#29)
            auto cmd = std::make_unique<SetFormulaCommand>(
                addr, formula, CellValue{CellError::VALUE}, old_val, old_formula);
            state.active_state().undo_manager.execute(std::move(cmd), sheet);
        }
    } else {
        state.active_state().dep_graph.set_dependencies(addr, {});
        CellValue new_val;

        auto date_result = parse_date(input);
        if (date_result) {
            new_val = CellValue{date_result->serial};
            CellFormat cur = state.active_state().format_map.get(addr);
            if (cur.type == FormatType::GENERAL) {
                CellFormat date_fmt;
                date_fmt.type = FormatType::DATE;
                date_fmt.date_input_hint = date_result->input_hint;
                state.active_state().format_map.set(addr, date_fmt);
            }
        } else {
            try {
                size_t pos = 0;
                double d = std::stod(input, &pos);
                if (pos == input.size())
                    new_val = CellValue{d};
                else
                    new_val = CellValue{input};
            } catch (...) {
                new_val = CellValue{input};
            }
        }

        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, old_val, old_formula);
        state.active_state().undo_manager.execute(std::move(cmd), sheet);
    }
}

// Batch recalc: given a set of changed cells, do one topological sort and
// re-evaluate all dependents that have formulas.
static void batch_recalc(Sheet& sheet, const std::unordered_set<CellAddress>& changed,
                          DependencyGraph& dep_graph, const FunctionRegistry& registry,
                          Workbook* workbook = nullptr) {
    auto order = dep_graph.recalc_order(changed);
    Evaluator eval(sheet, registry, workbook);
    for (const auto& cell : order) {
        if (!sheet.has_formula(cell)) continue;
        try {
            auto tokens = Tokenizer::tokenize(sheet.get_formula(cell));
            auto parsed = Parser::parse(tokens);
            sheet.set_value(cell, eval.evaluate(*parsed.root));
        } catch (...) {
            sheet.set_value(cell, CellValue{CellError::VALUE});
        }
    }
}

// ── File dialog helpers (simple path input via ImGui popup) ──────────────────

static char s_file_path_buf[512] = {};
static bool s_show_open = false;
static bool s_show_save = false;
static bool s_show_import_csv = false;
static bool s_show_export_csv = false;

// ── Menu bar ────────────────────────────────────────────────────────────────

void MainWindow::render_menu_bar(AppState& state) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                state.workbook = Workbook();
                state.sheet_states.clear();
                state.sheet_states.emplace_back();
                state.current_file.clear();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                s_show_open = true;
                std::strncpy(s_file_path_buf, state.current_file.c_str(), sizeof(s_file_path_buf) - 1);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!state.current_file.empty())
                    WorkbookIO::save(state.current_file, state.workbook);
                else
                    s_show_save = true;
            }
            if (ImGui::MenuItem("Save As...")) {
                s_show_save = true;
                std::strncpy(s_file_path_buf, state.current_file.c_str(), sizeof(s_file_path_buf) - 1);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import CSV...")) {
                s_show_import_csv = true;
                s_file_path_buf[0] = '\0';
            }
            if (ImGui::MenuItem("Export CSV...")) {
                s_show_export_csv = true;
                s_file_path_buf[0] = '\0';
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, state.active_state().undo_manager.can_undo())) {
                auto& sheet = state.workbook.active_sheet();
                state.active_state().undo_manager.undo(sheet);
                if (auto cell = state.active_state().undo_manager.last_affected())
                    recalc_dependents(sheet, *cell, state.active_state().dep_graph, state.function_registry, &state.workbook);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, state.active_state().undo_manager.can_redo())) {
                auto& sheet = state.workbook.active_sheet();
                state.active_state().undo_manager.redo(sheet);
                if (auto cell = state.active_state().undo_manager.last_affected())
                    recalc_dependents(sheet, *cell, state.active_state().dep_graph, state.function_registry, &state.workbook);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                if (grid_state_.has_range_selection)
                    state.clipboard.copy(state.workbook.active_sheet(), grid_state_.sel_min(), grid_state_.sel_max());
                else
                    state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
                state.clipboard.set_cut(false);
            }
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                if (grid_state_.has_range_selection)
                    state.clipboard.copy(state.workbook.active_sheet(), grid_state_.sel_min(), grid_state_.sel_max());
                else
                    state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
                state.clipboard.set_cut(true);
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, state.clipboard.has_data())) {
                auto entries = state.clipboard.paste_at(grid_state_.selected);
                auto& sheet = state.workbook.active_sheet();
                for (const auto& pe : entries) {
                    if (!pe.formula.empty()) {
                        process_cell_input(("=" + pe.formula).c_str(), sheet, pe.addr, state);
                    } else {
                        std::string display = to_display_string(pe.value);
                        process_cell_input(display.c_str(), sheet, pe.addr, state);
                    }
                }
                if (state.clipboard.is_cut()) {
                    std::unordered_set<CellAddress> dest_set;
                    for (const auto& pe : entries) dest_set.insert(pe.addr);
                    for (const auto& src : state.clipboard.source_cells()) {
                        if (dest_set.count(src) == 0)
                            process_cell_input("", sheet, src, state);
                    }
                    state.clipboard.clear();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            auto& addr = grid_state_.selected;
            CellFormat current = state.active_state().format_map.get(addr);

            if (ImGui::MenuItem("General", nullptr, current.type == FormatType::GENERAL)) {
                state.active_state().format_map.set(addr, {FormatType::GENERAL});
            }
            if (ImGui::MenuItem("Number (2 dp)", nullptr, current.type == FormatType::NUMBER)) {
                state.active_state().format_map.set(addr, {FormatType::NUMBER, 2});
            }
            if (ImGui::MenuItem("Percentage", nullptr, current.type == FormatType::PERCENTAGE)) {
                state.active_state().format_map.set(addr, {FormatType::PERCENTAGE, 1});
            }
            if (ImGui::MenuItem("Currency ($)", nullptr, current.type == FormatType::CURRENCY)) {
                state.active_state().format_map.set(addr, {FormatType::CURRENCY, 2, "$"});
            }
            if (ImGui::MenuItem("Scientific", nullptr, current.type == FormatType::SCIENTIFIC)) {
                state.active_state().format_map.set(addr, {FormatType::SCIENTIFIC, 2});
            }
            if (ImGui::MenuItem("Date (ISO)", nullptr, current.type == FormatType::DATE)) {
                state.active_state().format_map.set(addr, {FormatType::DATE});
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            bool chart_visible = chart_panel_.is_visible();
            if (ImGui::MenuItem("Chart Panel", nullptr, chart_visible)) {
                chart_panel_.set_visible(!chart_visible);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dark", nullptr, theme_ == Theme::Dark)) {
                    theme_ = Theme::Dark;
                    apply_theme(theme_);
                }
                if (ImGui::MenuItem("Light", nullptr, theme_ == Theme::Light)) {
                    theme_ = Theme::Light;
                    apply_theme(theme_);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // ── File dialogs (simple InputText popups) ──────────────────────────────
    auto file_popup = [&](const char* id, bool& show, auto callback) {
        if (show) ImGui::OpenPopup(id);
        if (ImGui::BeginPopupModal(id, &show, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Path", s_file_path_buf, sizeof(s_file_path_buf));
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                callback(std::string(s_file_path_buf));
                show = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                show = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    };

    file_popup("Open File", s_show_open, [&](const std::string& path) {
        if (WorkbookIO::load(path, state.workbook)) {
            state.current_file = path;
            state.sheet_states.clear();
            state.sheet_states.resize(state.workbook.sheet_count());
        }
    });

    file_popup("Save File", s_show_save, [&](const std::string& path) {
        if (WorkbookIO::save(path, state.workbook))
            state.current_file = path;
    });

    file_popup("Import CSV", s_show_import_csv, [&](const std::string& path) {
        auto& sheet = state.workbook.active_sheet();
        CsvIO::import_file(path, sheet);
        // Clear stale per-sheet state after import (#31)
        state.active_state().dep_graph.clear();
        state.active_state().format_map = FormatMap{};
        state.active_state().undo_manager.clear();
    });

    file_popup("Export CSV", s_show_export_csv, [&](const std::string& path) {
        CsvIO::export_file(path, state.workbook.active_sheet());
    });
}

// ── Keyboard shortcuts ──────────────────────────────────────────────────────

void MainWindow::handle_keyboard(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;

    // Don't handle shortcuts when typing in an input field
    if (ImGui::GetIO().WantTextInput) return;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        auto& sheet = state.workbook.active_sheet();
        state.active_state().undo_manager.undo(sheet);
        if (auto cell = state.active_state().undo_manager.last_affected())
            recalc_dependents(sheet, *cell, state.active_state().dep_graph, state.function_registry, &state.workbook);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        auto& sheet = state.workbook.active_sheet();
        state.active_state().undo_manager.redo(sheet);
        if (auto cell = state.active_state().undo_manager.last_affected())
            recalc_dependents(sheet, *cell, state.active_state().dep_graph, state.function_registry, &state.workbook);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        if (grid_state_.has_range_selection)
            state.clipboard.copy(state.workbook.active_sheet(), grid_state_.sel_min(), grid_state_.sel_max());
        else
            state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
        state.clipboard.set_cut(false);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        if (grid_state_.has_range_selection)
            state.clipboard.copy(state.workbook.active_sheet(), grid_state_.sel_min(), grid_state_.sel_max());
        else
            state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
        state.clipboard.set_cut(true);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V) && state.clipboard.has_data()) {
        auto entries = state.clipboard.paste_at(grid_state_.selected);
        auto& sheet = state.workbook.active_sheet();
        for (const auto& pe : entries) {
            if (!pe.formula.empty())
                process_cell_input(("=" + pe.formula).c_str(), sheet, pe.addr, state);
            else
                process_cell_input(to_display_string(pe.value).c_str(), sheet, pe.addr, state);
        }
        if (state.clipboard.is_cut()) {
            std::unordered_set<CellAddress> dest_set;
            for (const auto& pe : entries) dest_set.insert(pe.addr);
            for (const auto& src : state.clipboard.source_cells()) {
                if (dest_set.count(src) == 0)
                    process_cell_input("", sheet, src, state);
            }
            state.clipboard.clear();
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (!state.current_file.empty())
            WorkbookIO::save(state.current_file, state.workbook);
        else
            s_show_save = true;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        s_show_open = true;
    }

    // When not editing a cell
    if (!grid_state_.editor.is_editing()) {
        bool shift = io.KeyShift;

        // Arrow key navigation (Shift extends selection)
        auto move = [&](int dc, int dr) {
            if (!shift) {
                grid_state_.has_range_selection = false;
                grid_state_.sel_anchor = grid_state_.selected;
            } else if (!grid_state_.has_range_selection) {
                grid_state_.has_range_selection = true;
                grid_state_.sel_anchor = grid_state_.selected;
            }
            grid_state_.selected.col = std::max(0, grid_state_.selected.col + dc);
            grid_state_.selected.row = std::max(0, grid_state_.selected.row + dr);
        };

        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    move(0, -1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))   move(0, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))   move(-1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))  move(1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            grid_state_.has_range_selection = false;
            grid_state_.selected.col++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            grid_state_.has_range_selection = false;
            grid_state_.selected.row++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            auto& sheet = state.workbook.active_sheet();
            process_cell_input("", sheet, grid_state_.selected, state);
        }

        // F2 to edit current cell
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            auto& sheet = state.workbook.active_sheet();
            std::string initial;
            if (sheet.has_formula(grid_state_.selected))
                initial = "=" + sheet.get_formula(grid_state_.selected);
            else
                initial = to_display_string(sheet.get_value(grid_state_.selected));
            grid_state_.editor.begin_edit(grid_state_.selected, initial);
        }

        // Start typing on selected cell → begin editing
        // Seed buffer with first char (InputText won't process queue on activation frame)
        if (!ctrl && !io.KeyAlt) {
            ImGuiIO& input_io = ImGui::GetIO();
            if (input_io.InputQueueCharacters.Size > 0) {
                ImWchar wch = input_io.InputQueueCharacters[0];
                if (wch >= 32 && wch < 127) {
                    std::string seed(1, static_cast<char>(wch));
                    input_io.InputQueueCharacters.erase(input_io.InputQueueCharacters.Data);
                    grid_state_.editor.begin_edit(grid_state_.selected, seed, false);
                }
            }
        }
    }
}

// ── Main render ─────────────────────────────────────────────────────────────

void MainWindow::render(AppState& state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_MenuBar;

    ImGui::Begin("Magic Dashboard", nullptr, window_flags);

    render_menu_bar(state);
    handle_keyboard(state);

    // Ctrl+Scroll to zoom, Ctrl+0 to reset
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.WantTextInput) {
        if (io.MouseWheel != 0.0f) {
            zoom_ += io.MouseWheel * 0.1f;
            zoom_ = std::clamp(zoom_, 0.5f, 3.0f);
            io.MouseWheel = 0.0f;  // consume so grid doesn't scroll
        }
        if (ImGui::IsKeyPressed(ImGuiKey_0)) {
            zoom_ = 1.0f;
        }
    }
    io.FontGlobalScale = zoom_;

    // Apply any async recalc results
    auto async_results = state.async_recalc.poll_results();
    if (!async_results.empty()) {
        for (const auto& r : async_results) {
            state.workbook.active_sheet().set_value(r.addr, r.value);
        }
        // Force formula bar to refresh so it shows updated values (#30)
        formula_bar_.force_refresh();
    }

    Sheet& sheet = state.workbook.active_sheet();

    // Formula bar
    bool cell_editing = grid_state_.editor.is_editing();
    if (formula_bar_.render(sheet, grid_state_.selected, cell_editing)) {
        process_cell_input(formula_bar_.buffer(), sheet, grid_state_.selected, state);
    }

    ImGui::Separator();

    // Pass formula bar buffer to grid for reference highlighting
    grid_state_.formula_bar_buf = formula_bar_.is_formula_mode()
        ? formula_bar_.buffer() : nullptr;
    grid_state_.dark_theme = (theme_ == Theme::Dark);

    // Spreadsheet grid
    if (grid_.render(sheet, grid_state_, state.active_state().format_map)) {
        process_cell_input(grid_state_.editor.buffer(), sheet,
                          grid_state_.editor.editing_cell(), state);
    }

    // Handle cell drag completions (move / fill)
    if (grid_state_.drag_completed) {
        grid_state_.drag_completed = false;
        CellAddress src = grid_state_.drag_source;
        CellAddress tgt = grid_state_.drag_target;

        if (grid_state_.drag_mode == CellDragMode::Move && !(src == tgt)) {
            // Move: transfer source cell to target, clear source
            CellValue src_val = sheet.get_value(src);
            std::string src_formula = sheet.get_formula(src);

            // Set target cell
            if (!src_formula.empty()) {
                int32_t dcol = tgt.col - src.col;
                int32_t drow = tgt.row - src.row;
                std::string adjusted = Clipboard::adjust_references(src_formula, dcol, drow);
                process_cell_input(("=" + adjusted).c_str(), sheet, tgt, state);
            } else {
                std::string display = to_display_string(src_val);
                process_cell_input(display.c_str(), sheet, tgt, state);
            }
            // Clear source cell
            process_cell_input("", sheet, src, state);
            grid_state_.selected = tgt;
            grid_state_.sel_anchor = tgt;
            grid_state_.has_range_selection = false;
        } else if (grid_state_.drag_mode == CellDragMode::Fill && !(src == tgt)) {
            // Fill: 2D rectangular fill from source to target (batch recalc)
            CellValue src_val = sheet.get_value(src);
            std::string src_formula = sheet.get_formula(src);

            int32_t c1 = std::min(src.col, tgt.col);
            int32_t c2 = std::max(src.col, tgt.col);
            int32_t r1 = std::min(src.row, tgt.row);
            int32_t r2 = std::max(src.row, tgt.row);

            // Phase 1: set all cells without recalc
            std::unordered_set<CellAddress> changed_cells;
            for (int32_t c = c1; c <= c2; ++c) {
                for (int32_t r = r1; r <= r2; ++r) {
                    if (c == src.col && r == src.row) continue;
                    CellAddress fill_addr{c, r};
                    if (!src_formula.empty()) {
                        std::string adjusted = Clipboard::adjust_references(
                            src_formula, c - src.col, r - src.row);
                        set_cell_no_recalc(("=" + adjusted).c_str(), sheet, fill_addr, state);
                    } else {
                        std::string display = to_display_string(src_val);
                        set_cell_no_recalc(display.c_str(), sheet, fill_addr, state);
                    }
                    changed_cells.insert(fill_addr);
                }
            }

            // Phase 2: one batch recalc for all changed cells
            batch_recalc(sheet, changed_cells, state.active_state().dep_graph, state.function_registry, &state.workbook);
        }
        grid_state_.drag_mode = CellDragMode::None;
    }

    // Sheet tabs
    if (ImGui::BeginTabBar("##sheets")) {
        for (int i = 0; i < state.workbook.sheet_count(); ++i) {
            bool selected = (i == state.workbook.active_index());
            if (ImGui::BeginTabItem(state.workbook.sheet(i).name().c_str(),
                                     nullptr,
                                     selected ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (!selected) state.workbook.set_active(i);
                ImGui::EndTabItem();
            }
        }
        // "+" tab to add sheet
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            state.workbook.add_sheet();
            state.ensure_sheet_states();
        }
        ImGui::EndTabBar();
    }

    // Status bar
    ImGui::Separator();
    auto fmt = state.active_state().format_map.get(grid_state_.selected);
    const char* fmt_name = "General";
    switch (fmt.type) {
        case FormatType::NUMBER: fmt_name = "Number"; break;
        case FormatType::PERCENTAGE: fmt_name = "%"; break;
        case FormatType::CURRENCY: fmt_name = "$"; break;
        case FormatType::SCIENTIFIC: fmt_name = "Sci"; break;
        case FormatType::DATE: fmt_name = "Date"; break;
        default: break;
    }

    // Selection stats (SUM, AVG, COUNT) when range selected (cached)
    if (grid_state_.has_range_selection) {
        auto smin = grid_state_.sel_min();
        auto smax = grid_state_.sel_max();
        uint64_t gen = sheet.value_generation();
        if (!(smin == cached_sel_min_) || !(smax == cached_sel_max_) || gen != cached_sel_gen_) {
            cached_sel_min_ = smin;
            cached_sel_max_ = smax;
            cached_sel_gen_ = gen;
            cached_sum_ = 0.0;
            cached_count_ = 0;
            for (int32_t c = smin.col; c <= smax.col; ++c) {
                for (int32_t r = smin.row; r <= smax.row; ++r) {
                    auto v = sheet.get_value({c, r});
                    if (is_number(v)) {
                        cached_sum_ += as_number(v);
                        ++cached_count_;
                    }
                }
            }
        }
        cached_has_range_ = true;
        int zoom_pct = static_cast<int>(zoom_ * 100.0f + 0.5f);
        if (cached_count_ > 0) {
            ImGui::Text("Cell: %s:%s  |  SUM: %.4g  AVG: %.4g  COUNT: %d  |  Format: %s  |  Zoom: %d%%",
                        smin.to_a1().c_str(), smax.to_a1().c_str(),
                        cached_sum_, cached_sum_ / cached_count_, cached_count_, fmt_name, zoom_pct);
        } else {
            ImGui::Text("Cell: %s:%s  |  COUNT: 0  |  Format: %s  |  Zoom: %d%%",
                        smin.to_a1().c_str(), smax.to_a1().c_str(), fmt_name, zoom_pct);
        }
    } else {
        cached_has_range_ = false;
        int zoom_pct = static_cast<int>(zoom_ * 100.0f + 0.5f);
        ImGui::Text("Cell: %s  |  Format: %s  |  Sheets: %d  |  Rows: %d  |  Zoom: %d%%",
                    grid_state_.selected.to_a1().c_str(),
                    fmt_name,
                    state.workbook.sheet_count(),
                    sheet.row_count(),
                    zoom_pct);
    }

    ImGui::End();

    // ── Plugin trust confirmation modal ─────────────────────────────────────
    if (!state.pending_plugin_path.empty()) {
        // Cache hash on first frame modal opens (#32)
        if (state.pending_plugin_path != cached_plugin_path_) {
            cached_plugin_path_ = state.pending_plugin_path;
            cached_plugin_hash_ = PluginAllowlist::sha256_of_file(state.pending_plugin_path);
        }
        ImGui::OpenPopup("Trust Plugin?");
    }
    if (ImGui::BeginPopupModal("Trust Plugin?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("An untrusted plugin was dropped:");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", state.pending_plugin_path.c_str());
        ImGui::Spacing();
        ImGui::Text("SHA-256:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", cached_plugin_hash_.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Trust & Load", ImVec2(140, 0))) {
            state.plugin_manager.trust_and_load(state.pending_plugin_path);
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            state.pending_plugin_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Render chart panel as a separate dockable window
    chart_panel_.render(sheet);

    // Render plugin panels as dockable windows
    state.plugin_manager.render_panels();
}

}  // namespace magic
