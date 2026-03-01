#include "ui/MainWindow.hpp"
#include "core/Workbook.hpp"
#include "formula/FunctionRegistry.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/DependencyGraph.hpp"
#include <imgui.h>
#include <cstring>

namespace magic {

// Extract cell references from AST for dependency tracking
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

static DependencyGraph s_dep_graph;

static void process_cell_input(const char* buf, Sheet& sheet, const CellAddress& addr,
                                FunctionRegistry& registry) {
    std::string input(buf);

    if (input.empty()) {
        sheet.set_value(addr, CellValue{});
        sheet.clear_formula(addr);
        s_dep_graph.remove(addr);
        return;
    }

    if (input[0] == '=') {
        std::string formula = input.substr(1);
        sheet.set_formula(addr, formula);

        try {
            auto tokens = Tokenizer::tokenize(formula);
            auto ast = Parser::parse(tokens);

            // Extract dependencies
            std::vector<CellAddress> refs;
            collect_refs(*ast, refs);
            s_dep_graph.set_dependencies(addr, refs);

            // Evaluate
            Evaluator eval(sheet, registry);
            CellValue result = eval.evaluate(*ast);
            sheet.set_value(addr, result);

            // Recalculate dependents
            std::unordered_set<CellAddress> changed = {addr};
            auto order = s_dep_graph.recalc_order(changed);
            for (const auto& dep_cell : order) {
                if (dep_cell == addr) continue;
                if (!sheet.has_formula(dep_cell)) continue;
                try {
                    auto dep_tokens = Tokenizer::tokenize(sheet.get_formula(dep_cell));
                    auto dep_ast = Parser::parse(dep_tokens);
                    CellValue dep_result = eval.evaluate(*dep_ast);
                    sheet.set_value(dep_cell, dep_result);
                } catch (...) {
                    sheet.set_value(dep_cell, CellValue{CellError::VALUE});
                }
            }
        } catch (const std::exception&) {
            sheet.set_value(addr, CellValue{CellError::VALUE});
        }
    } else {
        sheet.clear_formula(addr);
        s_dep_graph.remove(addr);

        // Try to parse as number
        try {
            double d = std::stod(input);
            sheet.set_value(addr, CellValue{d});
        } catch (...) {
            sheet.set_value(addr, CellValue{input});
        }
    }
}

void MainWindow::render(Workbook& workbook, FunctionRegistry& registry) {
    // Full-window panel
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Magic Dashboard", nullptr, window_flags);

    Sheet& sheet = workbook.active_sheet();

    // Formula bar
    if (formula_bar_.render(sheet, grid_state_.selected)) {
        // Formula bar committed — get buffer content
        // We need the buffer from FormulaBar; using a simpler approach
    }

    ImGui::Separator();

    // Spreadsheet grid
    if (grid_.render(sheet, grid_state_)) {
        // Cell editor committed
        process_cell_input(grid_state_.editor.buffer(), sheet,
                          grid_state_.editor.editing_cell(), registry);
    }

    // Sheet tabs
    if (ImGui::BeginTabBar("##sheets")) {
        for (int i = 0; i < workbook.sheet_count(); ++i) {
            bool selected = (i == workbook.active_index());
            if (ImGui::BeginTabItem(workbook.sheet(i).name().c_str(),
                                     nullptr,
                                     selected ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (!selected) workbook.set_active(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Status bar
    ImGui::Separator();
    ImGui::Text("Cell: %s  |  Sheets: %d  |  Rows: %d",
                grid_state_.selected.to_a1().c_str(),
                workbook.sheet_count(),
                sheet.row_count());

    ImGui::End();
}

}  // namespace magic
