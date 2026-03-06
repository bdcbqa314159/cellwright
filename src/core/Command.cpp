#include "core/Command.hpp"
#include "core/Sheet.hpp"

namespace magic {

// ── SetValueCommand ─────────────────────────────────────────────────────────

SetValueCommand::SetValueCommand(CellAddress addr, CellValue new_val,
                                 CellValue old_val, std::string old_formula)
    : addr_(addr), new_val_(std::move(new_val)),
      old_val_(std::move(old_val)), old_formula_(std::move(old_formula)) {}

void SetValueCommand::execute(Sheet& sheet) {
    sheet.set_value(addr_, new_val_);
    sheet.clear_formula(addr_);
}

void SetValueCommand::undo(Sheet& sheet) {
    sheet.set_value(addr_, old_val_);
    if (!old_formula_.empty())
        sheet.set_formula(addr_, old_formula_);
    else
        sheet.clear_formula(addr_);
}

std::string SetValueCommand::description() const {
    return "Set " + addr_.to_a1();
}

// ── SetFormulaCommand ───────────────────────────────────────────────────────

SetFormulaCommand::SetFormulaCommand(CellAddress addr, std::string new_formula,
                                     CellValue new_result,
                                     CellValue old_val, std::string old_formula)
    : addr_(addr), new_formula_(std::move(new_formula)),
      new_result_(std::move(new_result)),
      old_val_(std::move(old_val)), old_formula_(std::move(old_formula)) {}

void SetFormulaCommand::execute(Sheet& sheet) {
    sheet.set_formula(addr_, new_formula_);
    sheet.set_value(addr_, new_result_);
}

void SetFormulaCommand::undo(Sheet& sheet) {
    sheet.set_value(addr_, old_val_);
    if (!old_formula_.empty())
        sheet.set_formula(addr_, old_formula_);
    else
        sheet.clear_formula(addr_);
}

std::string SetFormulaCommand::description() const {
    return "Formula " + addr_.to_a1();
}

// ── InsertRowCommand ────────────────────────────────────────────────────

void InsertRowCommand::execute(Sheet& sheet) { sheet.insert_row(row_); }
void InsertRowCommand::undo(Sheet& sheet) { sheet.delete_row(row_); }
std::string InsertRowCommand::description() const { return "Insert Row " + std::to_string(row_ + 1); }

// ── DeleteRowCommand ────────────────────────────────────────────────────

DeleteRowCommand::DeleteRowCommand(int32_t row, Sheet& sheet) : row_(row) {
    // Save all cell values and formulas in this row for undo
    for (int32_t c = 0; c < sheet.col_count(); ++c) {
        saved_values_.push_back(sheet.get_value({c, row}));
        if (sheet.has_formula({c, row}))
            saved_formulas_[c] = sheet.get_formula({c, row});
    }
}

void DeleteRowCommand::execute(Sheet& sheet) { sheet.delete_row(row_); }

void DeleteRowCommand::undo(Sheet& sheet) {
    sheet.insert_row(row_);
    for (int32_t c = 0; c < static_cast<int32_t>(saved_values_.size()); ++c) {
        sheet.set_value({c, row_}, saved_values_[c]);
        auto it = saved_formulas_.find(c);
        if (it != saved_formulas_.end())
            sheet.set_formula({c, row_}, it->second);
    }
}

std::string DeleteRowCommand::description() const { return "Delete Row " + std::to_string(row_ + 1); }

// ── InsertColumnCommand ─────────────────────────────────────────────────

void InsertColumnCommand::execute(Sheet& sheet) { sheet.insert_column(col_); }
void InsertColumnCommand::undo(Sheet& sheet) { sheet.delete_column(col_); }
std::string InsertColumnCommand::description() const { return "Insert Col " + CellAddress::col_to_letters(col_); }

// ── DeleteColumnCommand ─────────────────────────────────────────────────

DeleteColumnCommand::DeleteColumnCommand(int32_t col, Sheet& sheet) : col_(col) {
    // Save all cell values and formulas in this column for undo
    for (int32_t r = 0; r < sheet.column(col).size(); ++r) {
        saved_values_.push_back(sheet.get_value({col, r}));
        if (sheet.has_formula({col, r}))
            saved_formulas_[r] = sheet.get_formula({col, r});
    }
}

void DeleteColumnCommand::execute(Sheet& sheet) { sheet.delete_column(col_); }

void DeleteColumnCommand::undo(Sheet& sheet) {
    sheet.insert_column(col_);
    for (int32_t r = 0; r < static_cast<int32_t>(saved_values_.size()); ++r) {
        sheet.set_value({col_, r}, saved_values_[r]);
        auto it = saved_formulas_.find(r);
        if (it != saved_formulas_.end())
            sheet.set_formula({col_, r}, it->second);
    }
}

std::string DeleteColumnCommand::description() const { return "Delete Col " + CellAddress::col_to_letters(col_); }

// ── UndoManager ─────────────────────────────────────────────────────────────

void UndoManager::execute(CommandPtr cmd, Sheet& sheet) {
    cmd->execute(sheet);
    undo_stack_.push_back(std::move(cmd));
    redo_stack_.clear();
    ++generation_;
}

void UndoManager::undo(Sheet& sheet) {
    if (undo_stack_.empty()) return;
    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    last_affected_ = cmd->cell();
    cmd->undo(sheet);
    redo_stack_.push_back(std::move(cmd));
    ++generation_;
}

void UndoManager::redo(Sheet& sheet) {
    if (redo_stack_.empty()) return;
    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    last_affected_ = cmd->cell();
    cmd->execute(sheet);
    undo_stack_.push_back(std::move(cmd));
    ++generation_;
}

void UndoManager::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
}

}  // namespace magic
