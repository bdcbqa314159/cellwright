#pragma once
#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"
#include <memory>
#include <optional>
#include <string>
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

    uint64_t generation() const { return generation_; }

private:
    std::vector<CommandPtr> undo_stack_;
    std::vector<CommandPtr> redo_stack_;
    std::optional<CellAddress> last_affected_;
    uint64_t generation_ = 0;
};

}  // namespace magic
