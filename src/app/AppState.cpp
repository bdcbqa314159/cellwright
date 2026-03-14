#include "app/AppState.hpp"
#include "app/AutoSave.hpp"
#include "io/WorkbookIO.hpp"
#include "io/CsvIO.hpp"

namespace magic {

bool AppState::open_file(const std::string& path) {
    if (WorkbookIO::load(path, workbook)) {
        current_file = path;
        sheet_states.clear();
        sheet_states.resize(workbook.sheet_count());
        mark_saved();
        settings.add_recent_file(path);
        return true;
    }
    return false;
}

bool AppState::save_file(const std::string& path) {
    if (WorkbookIO::save(path, workbook)) {
        current_file = path;
        mark_saved();
        AutoSave::discard_recovery();  // manual save supersedes recovery file
        settings.add_recent_file(path);
        return true;
    }
    return false;
}

bool AppState::import_csv(const std::string& path) {
    auto& sheet = workbook.active_sheet();
    auto& as = active_state();
    if (CsvIO::import_file(path, sheet)) {
        as.dep_graph.clear();
        as.format_map = FormatMap{};
        as.undo_manager.clear();
        return true;
    }
    return false;
}

bool AppState::export_csv(const std::string& path) {
    return CsvIO::export_file(path, workbook.active_sheet());
}

}  // namespace magic
