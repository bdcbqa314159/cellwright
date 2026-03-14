#pragma once
#include "core/CellAddress.hpp"
#include "ui/AutocompletePopup.hpp"

namespace magic {

class Sheet;
class FunctionRegistry;

class FormulaBar {
public:
    // Render the formula bar. Returns true if the user committed a new value.
    // nav_target is set when user navigates via the name box.
    bool render(Sheet& sheet, const CellAddress& selected,
                bool cell_editing = false, const FunctionRegistry* registry = nullptr,
                ImFont* mono_font = nullptr,
                const char* cell_editor_buf = nullptr);
    const char* buffer() const { return buf_; }
    bool is_editing() const { return editing_; }
    bool took_focus() const { return took_focus_; }
    void clear_took_focus() { took_focus_ = false; }
    void insert_ref(const std::string& ref);
    bool is_formula_mode() const { return editing_ && buf_[0] == '='; }
    void force_refresh() { last_selected_ = {-1, -1}; }

    // Name box navigation: set to target cell after Enter in name box
    bool has_nav_target() const { return has_nav_target_; }
    CellAddress consume_nav_target() { has_nav_target_ = false; return nav_target_; }
    void focus_name_box() { focus_name_box_ = true; }

private:
    static constexpr std::size_t kFormulaBufferSize = 1024;
    char buf_[kFormulaBufferSize] = {};
    char name_buf_[32] = {};
    CellAddress last_selected_{-1, -1};
    bool editing_ = false;
    int cursor_pos_ = 0;
    AutocompletePopup autocomplete_;
    bool has_nav_target_ = false;
    bool focus_name_box_ = false;
    bool name_box_active_ = false;
    bool took_focus_ = false;
    int set_cursor_pos_ = -1;
    CellAddress nav_target_{0, 0};
};

}  // namespace magic
