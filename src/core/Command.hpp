#pragma once
#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace magic {

class Sheet;

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Sheet& sheet) = 0;
    virtual void undo(Sheet& sheet) = 0;
    virtual std::string description() const = 0;
    virtual CellAddress cell() const = 0;
};

using CommandPtr = std::unique_ptr<Command>;

// Set a cell value (non-formula)
class SetValueCommand : public Command {
public:
    SetValueCommand(CellAddress addr, CellValue new_val,
                    CellValue old_val, std::string old_formula = "");

    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return addr_; }

private:
    CellAddress addr_;
    CellValue new_val_;
    CellValue old_val_;
    std::string old_formula_;
};

// Set a formula
class SetFormulaCommand : public Command {
public:
    SetFormulaCommand(CellAddress addr, std::string new_formula, CellValue new_result,
                      CellValue old_val, std::string old_formula);

    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return addr_; }

private:
    CellAddress addr_;
    std::string new_formula_;
    CellValue new_result_;
    CellValue old_val_;
    std::string old_formula_;
};

// Insert/delete row commands
class InsertRowCommand : public Command {
public:
    InsertRowCommand(int32_t row) : row_(row) {}
    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return {0, row_}; }
private:
    int32_t row_;
};

class DeleteRowCommand : public Command {
public:
    DeleteRowCommand(int32_t row, Sheet& sheet);
    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return {0, row_}; }
private:
    int32_t row_;
    std::vector<CellValue> saved_values_;
    std::unordered_map<int32_t, std::string> saved_formulas_;  // col → formula
};

class InsertColumnCommand : public Command {
public:
    InsertColumnCommand(int32_t col) : col_(col) {}
    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return {col_, 0}; }
private:
    int32_t col_;
};

class DeleteColumnCommand : public Command {
public:
    DeleteColumnCommand(int32_t col, Sheet& sheet);
    void execute(Sheet& sheet) override;
    void undo(Sheet& sheet) override;
    std::string description() const override;
    CellAddress cell() const override { return {col_, 0}; }
private:
    int32_t col_;
    std::vector<CellValue> saved_values_;
    std::unordered_map<int32_t, std::string> saved_formulas_;  // row → formula
};

// Undo/redo manager
class UndoManager {
public:
    void execute(CommandPtr cmd, Sheet& sheet);
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }
    void undo(Sheet& sheet);
    void redo(Sheet& sheet);
    void clear();
    std::optional<CellAddress> last_affected() const { return last_affected_; }

    std::string peek_undo_desc() const {
        return undo_stack_.empty() ? "" : undo_stack_.back()->description();
    }
    std::string peek_redo_desc() const {
        return redo_stack_.empty() ? "" : redo_stack_.back()->description();
    }

    uint64_t generation() const { return generation_; }

private:
    std::vector<CommandPtr> undo_stack_;
    std::vector<CommandPtr> redo_stack_;
    std::optional<CellAddress> last_affected_;
    uint64_t generation_ = 0;
};

}  // namespace magic
