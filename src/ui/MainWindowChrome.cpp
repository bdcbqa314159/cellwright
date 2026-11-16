// Part of MainWindow, split out of MainWindow.cpp purely to keep each translation
// unit readable (issue #17). Same class, same behaviour, and no declaration in
// MainWindow.hpp changed. Sheet tabs and status bar.

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <nfd.hpp>

#include "core/ConditionalFormat.hpp"
#include "core/FillPattern.hpp"
#include "core/Workbook.hpp"
#include "formula/AsyncRecalcEngine.hpp"
#include "io/CsvIO.hpp"
#include "io/WorkbookIO.hpp"
#include "services/Clipboard.hpp"
#include "session/AutoSave.hpp"
#include "session/SessionState.hpp"
#include "ui/MainWindow.hpp"
#include "util/Sha256.hpp"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace magic {

// ── Shared action methods ────────────────────────────────────────────────────

void MainWindow::render_sheet_tabs(SessionState& state) {
    if (ImGui::BeginTabBar("##sheets")) {
        for (int i = 0; i < state.workbook.sheet_count(); ++i) {
            bool selected = (i == state.workbook.active_index());
            if (ImGui::BeginTabItem(state.workbook.sheet(i).name().c_str(), nullptr,
                                    selected ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (!selected) state.workbook.set_active(i);
                ImGui::EndTabItem();
            }
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            state.workbook.add_sheet();
            state.ensure_sheet_states();
        }
        ImGui::EndTabBar();
    }
}

void MainWindow::render_status_bar(SessionState& state, Sheet& sheet) {
    ImGui::Separator();
    auto& as = state.active_state();
    auto fmt = as.format_map.get(grid_state_.selection.selected_cell);
    const char* fmt_name = "General";
    switch (fmt.type) {
        case FormatType::NUMBER:
            fmt_name = "Number";
            break;
        case FormatType::PERCENTAGE:
            fmt_name = "%";
            break;
        case FormatType::CURRENCY:
            fmt_name = "$";
            break;
        case FormatType::SCIENTIFIC:
            fmt_name = "Sci";
            break;
        case FormatType::DATE:
            fmt_name = "Date";
            break;
        default:
            break;
    }

    if (grid_state_.selection.has_range) {
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
            ImGui::Text(
                "Cell: %s:%s  |  SUM: %.4g  AVG: %.4g  COUNT: %d  |  Format: %s  |  Zoom: %d%%",
                smin.to_a1().c_str(), smax.to_a1().c_str(), cached_sum_,
                cached_sum_ / cached_count_, cached_count_, fmt_name, zoom_pct);
        } else {
            ImGui::Text("Cell: %s:%s  |  COUNT: 0  |  Format: %s  |  Zoom: %d%%",
                        smin.to_a1().c_str(), smax.to_a1().c_str(), fmt_name, zoom_pct);
        }
    } else {
        cached_has_range_ = false;
        int zoom_pct = static_cast<int>(zoom_ * 100.0f + 0.5f);
        ImGui::Text("Cell: %s  |  Format: %s  |  Sheets: %d  |  Rows: %d  |  Zoom: %d%%",
                    grid_state_.selection.selected_cell.to_a1().c_str(), fmt_name,
                    state.workbook.sheet_count(), sheet.row_count(), zoom_pct);
    }
}

}  // namespace magic
