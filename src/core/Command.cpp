#include "core/Command.hpp"
#include "core/Sheet.hpp"
#include <algorithm>
#include <numeric>
#include <cctype>

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
    // Save cells in the deleted row
    for (int32_t c = 0; c < sheet.col_count(); ++c) {
        saved_row_values_.push_back(sheet.get_value({c, row}));
        if (sheet.has_formula({c, row}))
            saved_row_formulas_[c] = sheet.get_formula({c, row});
    }
    // Snapshot all formulas so undo can restore them exactly
    // (delete_row adjusts formula text irreversibly, e.g. =A4 → #REF!)
    pre_delete_formulas_ = sheet.all_formulas();
}

void DeleteRowCommand::execute(Sheet& sheet) { sheet.delete_row(row_); }

void DeleteRowCommand::undo(Sheet& sheet) {
    sheet.insert_row(row_);
    // Restore cells in the re-inserted row
    for (int32_t c = 0; c < static_cast<int32_t>(saved_row_values_.size()); ++c) {
        sheet.set_value({c, row_}, saved_row_values_[c]);
    }
    // Restore the exact pre-delete formula map (overrides insert_row's adjustment)
    sheet.set_all_formulas(pre_delete_formulas_);
}

std::string DeleteRowCommand::description() const { return "Delete Row " + std::to_string(row_ + 1); }

// ── InsertColumnCommand ─────────────────────────────────────────────────

void InsertColumnCommand::execute(Sheet& sheet) { sheet.insert_column(col_); }
void InsertColumnCommand::undo(Sheet& sheet) { sheet.delete_column(col_); }
std::string InsertColumnCommand::description() const { return "Insert Col " + CellAddress::col_to_letters(col_); }

// ── DeleteColumnCommand ─────────────────────────────────────────────────

DeleteColumnCommand::DeleteColumnCommand(int32_t col, Sheet& sheet) : col_(col) {
    // Save cells in the deleted column
    for (int32_t r = 0; r < sheet.column(col).size(); ++r) {
        saved_col_values_.push_back(sheet.get_value({col, r}));
    }
    // Snapshot all formulas so undo can restore them exactly
    pre_delete_formulas_ = sheet.all_formulas();
}

void DeleteColumnCommand::execute(Sheet& sheet) { sheet.delete_column(col_); }

void DeleteColumnCommand::undo(Sheet& sheet) {
    sheet.insert_column(col_);
    // Restore cells in the re-inserted column
    for (int32_t r = 0; r < static_cast<int32_t>(saved_col_values_.size()); ++r) {
        sheet.set_value({col_, r}, saved_col_values_[r]);
    }
    // Restore the exact pre-delete formula map
    sheet.set_all_formulas(pre_delete_formulas_);
}

std::string DeleteColumnCommand::description() const { return "Delete Col " + CellAddress::col_to_letters(col_); }

// ── SortColumnCommand ──────────────────────────────────────────────────────
// NOTE: The constructor snapshots all column values for undo. This is correct
// but expensive for large sheets — an optimization target for future work.

// Sort priority: empty last, numbers, bools, strings, errors
static int sort_rank(const CellValue& v) {
    if (is_empty(v)) return 4;  // always last
    if (is_number(v)) return 0;
    if (is_bool(v)) return 1;
    if (is_string(v)) return 2;
    return 3;  // errors
}

SortColumnCommand::SortColumnCommand(int32_t sort_col, bool ascending, Sheet& sheet)
    : sort_col_(sort_col), ascending_(ascending)
{
    // Find data extent (max row across all columns that has non-empty data)
    data_rows_ = 0;
    for (int32_t c = 0; c < sheet.col_count(); ++c)
        data_rows_ = std::max(data_rows_, sheet.column(c).size());
    if (data_rows_ <= 0) return;

    // Snapshot all values and formulas for undo
    saved_values_.resize(sheet.col_count());
    for (int32_t c = 0; c < sheet.col_count(); ++c) {
        saved_values_[c].resize(data_rows_);
        for (int32_t r = 0; r < data_rows_; ++r)
            saved_values_[c][r] = sheet.get_value({c, r});
    }
    saved_formulas_ = sheet.all_formulas();

    // Build permutation by sorting the sort column
    permutation_.resize(data_rows_);
    std::iota(permutation_.begin(), permutation_.end(), 0);

    std::stable_sort(permutation_.begin(), permutation_.end(),
        [&](int32_t a, int32_t b) {
            CellValue va = sheet.get_value({sort_col_, a});
            CellValue vb = sheet.get_value({sort_col_, b});
            int ra = sort_rank(va), rb = sort_rank(vb);
            if (ra != rb) return ra < rb;  // empty always last regardless of direction
            if (is_empty(va)) return false; // both empty = equal
            if (is_number(va) && is_number(vb)) {
                double da = as_number(va), db = as_number(vb);
                return ascending_ ? da < db : da > db;
            }
            if (is_bool(va) && is_bool(vb)) {
                int ia = std::get<bool>(va) ? 1 : 0;
                int ib = std::get<bool>(vb) ? 1 : 0;
                return ascending_ ? ia < ib : ia > ib;
            }
            if (is_string(va) && is_string(vb)) {
                const auto& sa = std::get<std::string>(va);
                const auto& sb = std::get<std::string>(vb);
                auto cmp = [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) <
                           std::tolower(static_cast<unsigned char>(b));
                };
                bool less = std::lexicographical_compare(
                    sa.begin(), sa.end(), sb.begin(), sb.end(), cmp);
                bool greater = std::lexicographical_compare(
                    sb.begin(), sb.end(), sa.begin(), sa.end(), cmp);
                return ascending_ ? less : greater;
            }
            return false;  // errors: preserve order
        });
}

void SortColumnCommand::execute(Sheet& sheet) {
    if (data_rows_ <= 0) return;

    // Apply permutation: build permuted data, then write back
    int32_t ncols = sheet.col_count();
    std::vector<std::vector<CellValue>> permuted(ncols);
    for (int32_t c = 0; c < ncols; ++c) {
        permuted[c].resize(data_rows_);
        for (int32_t r = 0; r < data_rows_; ++r)
            permuted[c][r] = sheet.get_value({c, permutation_[r]});
    }

    // Permute formulas
    std::unordered_map<CellAddress, std::string> new_formulas;
    for (int32_t r = 0; r < data_rows_; ++r) {
        for (int32_t c = 0; c < ncols; ++c) {
            CellAddress old_addr{c, permutation_[r]};
            auto it = sheet.all_formulas().find(old_addr);
            if (it != sheet.all_formulas().end())
                new_formulas[{c, r}] = it->second;
        }
    }
    // Keep formulas outside the sorted range
    for (auto& [addr, formula] : sheet.all_formulas()) {
        if (addr.row >= data_rows_)
            new_formulas[addr] = formula;
    }

    // Write permuted values
    for (int32_t c = 0; c < ncols; ++c)
        for (int32_t r = 0; r < data_rows_; ++r)
            sheet.set_value({c, r}, permuted[c][r]);

    sheet.set_all_formulas(std::move(new_formulas));
}

void SortColumnCommand::undo(Sheet& sheet) {
    if (data_rows_ <= 0) return;

    // Restore all values from snapshot
    for (int32_t c = 0; c < static_cast<int32_t>(saved_values_.size()); ++c)
        for (int32_t r = 0; r < data_rows_; ++r)
            sheet.set_value({c, r}, saved_values_[c][r]);

    sheet.set_all_formulas(saved_formulas_);
}

std::string SortColumnCommand::description() const {
    return "Sort " + CellAddress::col_to_letters(sort_col_) +
           (ascending_ ? " A-Z" : " Z-A");
}

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
