#include "app/AppState.hpp"
#include "io/WorkbookIO.hpp"

namespace magic {

bool AppState::open_file(const std::string& path) {
    if (WorkbookIO::load(path, workbook)) {
        current_file = path;
        sheet_states.clear();
        sheet_states.resize(workbook.sheet_count());
        mark_saved();
        return true;
    }
    return false;
}

}  // namespace magic
