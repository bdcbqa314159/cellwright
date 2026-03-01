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

// ── UndoManager ─────────────────────────────────────────────────────────────

void UndoManager::execute(CommandPtr cmd, Sheet& sheet) {
    cmd->execute(sheet);
    undo_stack_.push_back(std::move(cmd));
    redo_stack_.clear();
}

void UndoManager::undo(Sheet& sheet) {
    if (undo_stack_.empty()) return;
    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    cmd->undo(sheet);
    redo_stack_.push_back(std::move(cmd));
}

void UndoManager::redo(Sheet& sheet) {
    if (redo_stack_.empty()) return;
    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    cmd->execute(sheet);
    undo_stack_.push_back(std::move(cmd));
}

void UndoManager::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
}

}  // namespace magic
