#include "ui/MainWindow.hpp"
#include "app/AppState.hpp"
#include "core/Workbook.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
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
            auto dep_ast = Parser::parse(dep_tokens);
            sheet.set_value(dep_cell, eval.evaluate(*dep_ast));
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
        state.undo_manager.execute(std::move(cmd), sheet);
        state.dep_graph.remove(addr);
        return;
    }

    if (input[0] == '=') {
        std::string formula = input.substr(1);
        try {
            auto tokens = Tokenizer::tokenize(formula);
            auto ast = Parser::parse(tokens);

            std::vector<CellAddress> refs;
            collect_refs(*ast, refs);
            state.dep_graph.set_dependencies(addr, refs);

            Evaluator eval(sheet, state.function_registry, &state.workbook);
            CellValue result = eval.evaluate(*ast);

            auto cmd = std::make_unique<SetFormulaCommand>(addr, formula, result, old_val, old_formula);
            state.undo_manager.execute(std::move(cmd), sheet);

            recalc_dependents(sheet, addr, state.dep_graph, state.function_registry, &state.workbook);
        } catch (const std::exception&) {
            auto cmd = std::make_unique<SetFormulaCommand>(
                addr, formula, CellValue{CellError::VALUE}, old_val, old_formula);
            state.undo_manager.execute(std::move(cmd), sheet);
        }
    } else {
        state.dep_graph.remove(addr);
        CellValue new_val;
        try {
            double d = std::stod(input);
            new_val = CellValue{d};
        } catch (...) {
            new_val = CellValue{input};
        }
        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, old_val, old_formula);
        state.undo_manager.execute(std::move(cmd), sheet);
        recalc_dependents(sheet, addr, state.dep_graph, state.function_registry, &state.workbook);
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
                state.undo_manager.clear();
                state.dep_graph.clear();
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
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, state.undo_manager.can_undo())) {
                state.undo_manager.undo(state.workbook.active_sheet());
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, state.undo_manager.can_redo())) {
                state.undo_manager.redo(state.workbook.active_sheet());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
                state.clipboard.set_cut(false);
            }
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
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
                    // Clear source cell
                    // (for single-cell cut, source = selected before paste)
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            auto& addr = grid_state_.selected;
            CellFormat current = state.format_map.get(addr);

            if (ImGui::MenuItem("General", nullptr, current.type == FormatType::GENERAL)) {
                state.format_map.set(addr, {FormatType::GENERAL});
            }
            if (ImGui::MenuItem("Number (2 dp)", nullptr, current.type == FormatType::NUMBER)) {
                state.format_map.set(addr, {FormatType::NUMBER, 2});
            }
            if (ImGui::MenuItem("Percentage", nullptr, current.type == FormatType::PERCENTAGE)) {
                state.format_map.set(addr, {FormatType::PERCENTAGE, 1});
            }
            if (ImGui::MenuItem("Currency ($)", nullptr, current.type == FormatType::CURRENCY)) {
                state.format_map.set(addr, {FormatType::CURRENCY, 2, "$"});
            }
            if (ImGui::MenuItem("Scientific", nullptr, current.type == FormatType::SCIENTIFIC)) {
                state.format_map.set(addr, {FormatType::SCIENTIFIC, 2});
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
            state.undo_manager.clear();
            state.dep_graph.clear();
        }
    });

    file_popup("Save File", s_show_save, [&](const std::string& path) {
        if (WorkbookIO::save(path, state.workbook))
            state.current_file = path;
    });

    file_popup("Import CSV", s_show_import_csv, [&](const std::string& path) {
        auto& sheet = state.workbook.active_sheet();
        CsvIO::import_file(path, sheet);
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
        state.undo_manager.undo(state.workbook.active_sheet());
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        state.undo_manager.redo(state.workbook.active_sheet());
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        state.clipboard.copy_single(state.workbook.active_sheet(), grid_state_.selected);
        state.clipboard.set_cut(false);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
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

        // Start typing on selected cell → begin editing with that character
        if (!ctrl && !io.KeyAlt) {
            ImGuiIO& input_io = ImGui::GetIO();
            if (input_io.InputQueueCharacters.Size > 0) {
                char ch = static_cast<char>(input_io.InputQueueCharacters[0]);
                if (ch >= 32 && ch < 127) {
                    std::string initial(1, ch);
                    grid_state_.editor.begin_edit(grid_state_.selected, initial);
                    // Clear the character queue so it doesn't double-input
                    input_io.InputQueueCharacters.clear();
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

    // Apply any async recalc results
    auto async_results = state.async_recalc.poll_results();
    for (const auto& r : async_results) {
        state.workbook.active_sheet().set_value(r.addr, r.value);
    }

    Sheet& sheet = state.workbook.active_sheet();

    // Formula bar
    if (formula_bar_.render(sheet, grid_state_.selected)) {
        // Committed from formula bar — handled via buffer
    }

    ImGui::Separator();

    // Spreadsheet grid
    if (grid_.render(sheet, grid_state_, state.format_map)) {
        process_cell_input(grid_state_.editor.buffer(), sheet,
                          grid_state_.editor.editing_cell(), state);
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
        }
        ImGui::EndTabBar();
    }

    // Status bar
    ImGui::Separator();
    auto fmt = state.format_map.get(grid_state_.selected);
    const char* fmt_name = "General";
    switch (fmt.type) {
        case FormatType::NUMBER: fmt_name = "Number"; break;
        case FormatType::PERCENTAGE: fmt_name = "%"; break;
        case FormatType::CURRENCY: fmt_name = "$"; break;
        case FormatType::SCIENTIFIC: fmt_name = "Sci"; break;
        default: break;
    }

    // Selection stats (SUM, AVG, COUNT) when range selected
    if (grid_state_.has_range_selection) {
        auto smin = grid_state_.sel_min();
        auto smax = grid_state_.sel_max();
        double sum = 0.0;
        int count = 0;
        for (int32_t c = smin.col; c <= smax.col; ++c) {
            for (int32_t r = smin.row; r <= smax.row; ++r) {
                auto v = sheet.get_value({c, r});
                if (is_number(v)) {
                    sum += as_number(v);
                    ++count;
                }
            }
        }
        if (count > 0) {
            ImGui::Text("Cell: %s:%s  |  SUM: %.4g  AVG: %.4g  COUNT: %d  |  Format: %s",
                        smin.to_a1().c_str(), smax.to_a1().c_str(),
                        sum, sum / count, count, fmt_name);
        } else {
            ImGui::Text("Cell: %s:%s  |  COUNT: 0  |  Format: %s",
                        smin.to_a1().c_str(), smax.to_a1().c_str(), fmt_name);
        }
    } else {
        ImGui::Text("Cell: %s  |  Format: %s  |  Sheets: %d  |  Rows: %d",
                    grid_state_.selected.to_a1().c_str(),
                    fmt_name,
                    state.workbook.sheet_count(),
                    sheet.row_count());
    }

    ImGui::End();

    // Render plugin panels as dockable windows
    state.plugin_manager.render_panels();
}

}  // namespace magic
