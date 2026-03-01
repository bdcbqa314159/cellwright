#pragma once
#include "core/CellAddress.hpp"
#include <string>

namespace magic {

class Sheet;

class CellEditor {
public:
    bool is_editing() const { return editing_; }
    const CellAddress& editing_cell() const { return cell_; }
    bool is_formula_mode() const { return editing_ && buf_[0] == '='; }

    // select_all=true for F2/double-click, false for type-to-edit
    void begin_edit(const CellAddress& cell, const std::string& initial, bool select_all = true);
    bool render();  // returns true on commit
    void cancel();
    void insert_ref(const std::string& ref);

    const char* buffer() const { return buf_; }

private:
    bool editing_ = false;
    bool focus_needed_ = false;
    bool select_all_ = true;
    bool cursor_to_end_ = false;
    int frames_since_start_ = 0;
    CellAddress cell_;
    char buf_[1024] = {};
};

}  // namespace magic
