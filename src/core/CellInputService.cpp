#include "core/CellInputService.hpp"
#include "core/Sheet.hpp"
#include "core/Workbook.hpp"
#include "core/Clipboard.hpp"
#include "core/Command.hpp"
#include "core/CellFormat.hpp"
#include "core/DateSerial.hpp"
#include "formula/Tokenizer.hpp"
#include "formula/Parser.hpp"
#include "formula/Evaluator.hpp"
#include "formula/DependencyGraph.hpp"

namespace magic {

// ── AST reference collector ─────────────────────────────────────────────────

static constexpr int MAX_COLLECT_DEPTH = 256;

static void collect_refs(const ASTNode& node, std::vector<CellAddress>& refs, int depth = 0) {
    if (depth >= MAX_COLLECT_DEPTH) return;
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CellRefNode>) {
            refs.push_back(v.addr);
        } else if constexpr (std::is_same_v<T, RangeNode>) {
            int32_t c1 = std::min(v.from.col, v.to.col);
            int32_t c2 = std::max(v.from.col, v.to.col);
            int32_t r1 = std::min(v.from.row, v.to.row);
            int32_t r2 = std::max(v.from.row, v.to.row);
            for (int32_t c = c1; c <= c2; ++c)
                for (int32_t r = r1; r <= r2; ++r)
                    refs.push_back({c, r});
        } else if constexpr (std::is_same_v<T, BinOpNode>) {
            collect_refs(*v.left, refs, depth + 1);
            collect_refs(*v.right, refs, depth + 1);
        } else if constexpr (std::is_same_v<T, UnaryOpNode>) {
            collect_refs(*v.operand, refs, depth + 1);
        } else if constexpr (std::is_same_v<T, FuncCallNode>) {
            for (const auto& arg : v.args) collect_refs(*arg, refs, depth + 1);
        } else if constexpr (std::is_same_v<T, CompareNode>) {
            collect_refs(*v.left, refs, depth + 1);
            collect_refs(*v.right, refs, depth + 1);
        }
    }, node.value);
}

// ── Constructor ─────────────────────────────────────────────────────────────

CellInputService::CellInputService(FunctionRegistry& registry)
    : registry_(registry) {}

// ── recalc_dependents ───────────────────────────────────────────────────────

void CellInputService::recalc_dependents(Sheet& sheet, const CellAddress& addr,
                                          DependencyGraph& dep_graph, Workbook* workbook) {
    std::unordered_set<CellAddress> changed = {addr};
    auto plan = dep_graph.recalc_order(changed);
    Evaluator eval(sheet, registry_, workbook);
    for (const auto& dep_cell : plan.order) {
        if (dep_cell == addr) continue;
        if (!sheet.has_formula(dep_cell)) continue;
        try {
            auto dep_tokens = Tokenizer::tokenize(sheet.get_formula(dep_cell));
            auto dep_formula = Parser::parse(dep_tokens);
            sheet.set_value(dep_cell, eval.evaluate(*dep_formula.root));
        } catch (...) {
            sheet.set_value(dep_cell, CellValue{CellError::VALUE});
        }
    }
    // Mark cycle cells as REF errors
    for (const auto& cell : plan.cycles) {
        sheet.set_value(cell, CellValue{CellError::REF});
    }
}

// ── process_impl (shared by process and process_no_recalc) ──────────────────

void CellInputService::process_impl(const char* buf, Sheet& sheet, const CellAddress& addr,
                                     UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                                     Workbook& workbook, bool do_recalc) {
    std::string input(buf);
    CellValue old_val = sheet.get_value(addr);
    std::string old_formula = sheet.get_formula(addr);

    if (input.empty()) {
        auto cmd = std::make_unique<SetValueCommand>(addr, CellValue{}, old_val, old_formula);
        undo.execute(std::move(cmd), sheet);
        dep_graph.set_dependencies(addr, {});
        if (do_recalc)
            recalc_dependents(sheet, addr, dep_graph, &workbook);
        return;
    }

    if (input[0] == '=') {
        std::string formula = input.substr(1);
        try {
            auto tokens = Tokenizer::tokenize(formula);
            auto parsed = Parser::parse(tokens);

            std::vector<CellAddress> refs;
            collect_refs(*parsed.root, refs);
            dep_graph.set_dependencies(addr, refs);

            Evaluator eval(sheet, registry_, &workbook);
            CellValue result = eval.evaluate(*parsed.root);

            auto cmd = std::make_unique<SetFormulaCommand>(addr, formula, result, old_val, old_formula);
            undo.execute(std::move(cmd), sheet);

            if (do_recalc)
                recalc_dependents(sheet, addr, dep_graph, &workbook);
        } catch (const std::exception&) {
            dep_graph.set_dependencies(addr, {});
            auto cmd = std::make_unique<SetFormulaCommand>(
                addr, formula, CellValue{CellError::VALUE}, old_val, old_formula);
            undo.execute(std::move(cmd), sheet);
        }
    } else {
        dep_graph.set_dependencies(addr, {});
        CellValue new_val;

        auto date_result = parse_date(input);
        if (date_result) {
            new_val = CellValue{date_result->serial};
            CellFormat cur = formats.get(addr);
            if (cur.type == FormatType::GENERAL) {
                CellFormat date_fmt;
                date_fmt.type = FormatType::DATE;
                date_fmt.date_input_hint = date_result->input_hint;
                formats.set(addr, date_fmt);
            }
        } else {
            try {
                size_t pos = 0;
                double d = std::stod(input, &pos);
                if (pos == input.size())
                    new_val = CellValue{d};
                else
                    new_val = CellValue{input};
            } catch (...) {
                new_val = CellValue{input};
            }
        }

        auto cmd = std::make_unique<SetValueCommand>(addr, new_val, old_val, old_formula);
        undo.execute(std::move(cmd), sheet);
        if (do_recalc)
            recalc_dependents(sheet, addr, dep_graph, &workbook);
    }
}

// ── process ─────────────────────────────────────────────────────────────────

void CellInputService::process(const char* buf, Sheet& sheet, const CellAddress& addr,
                                UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                                Workbook& workbook) {
    process_impl(buf, sheet, addr, undo, formats, dep_graph, workbook, true);
}

// ── process_no_recalc ───────────────────────────────────────────────────────

void CellInputService::process_no_recalc(const char* buf, Sheet& sheet, const CellAddress& addr,
                                          UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                                          Workbook& workbook) {
    process_impl(buf, sheet, addr, undo, formats, dep_graph, workbook, false);
}

// ── batch_recalc ────────────────────────────────────────────────────────────

void CellInputService::batch_recalc(Sheet& sheet, const std::unordered_set<CellAddress>& changed,
                                     DependencyGraph& dep_graph, Workbook* workbook) {
    auto plan = dep_graph.recalc_order(changed);
    Evaluator eval(sheet, registry_, workbook);
    for (const auto& cell : plan.order) {
        if (!sheet.has_formula(cell)) continue;
        try {
            auto tokens = Tokenizer::tokenize(sheet.get_formula(cell));
            auto parsed = Parser::parse(tokens);
            sheet.set_value(cell, eval.evaluate(*parsed.root));
        } catch (...) {
            sheet.set_value(cell, CellValue{CellError::VALUE});
        }
    }
    // Mark cycle cells as REF errors
    for (const auto& cell : plan.cycles) {
        sheet.set_value(cell, CellValue{CellError::REF});
    }
}

// ── Clipboard operations ────────────────────────────────────────────────────

void CellInputService::copy(Clipboard& clipboard, const Sheet& sheet,
                             const CellAddress& selected, bool has_range,
                             const CellAddress& range_min, const CellAddress& range_max) {
    if (has_range)
        clipboard.copy(sheet, range_min, range_max);
    else
        clipboard.copy_single(sheet, selected);
    clipboard.set_cut(false);
}

void CellInputService::cut(Clipboard& clipboard, const Sheet& sheet,
                            const CellAddress& selected, bool has_range,
                            const CellAddress& range_min, const CellAddress& range_max) {
    if (has_range)
        clipboard.copy(sheet, range_min, range_max);
    else
        clipboard.copy_single(sheet, selected);
    clipboard.set_cut(true);
}

void CellInputService::paste(Clipboard& clipboard, Sheet& sheet, const CellAddress& dest,
                              UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                              Workbook& workbook) {
    auto entries = clipboard.paste_at(dest);
    for (const auto& pe : entries) {
        if (!pe.formula.empty()) {
            process(("=" + pe.formula).c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
        } else {
            std::string display = to_display_string(pe.value);
            process(display.c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
        }
    }
    if (clipboard.is_cut()) {
        std::unordered_set<CellAddress> dest_set;
        for (const auto& pe : entries) dest_set.insert(pe.addr);
        for (const auto& src : clipboard.source_cells()) {
            if (dest_set.count(src) == 0)
                process("", sheet, src, undo, formats, dep_graph, workbook);
        }
        clipboard.clear();
    }
}

void CellInputService::paste_special(Clipboard& clipboard, Sheet& sheet, const CellAddress& dest,
                                      UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                                      Workbook& workbook, PasteMode mode) {
    if (mode == PasteMode::Normal) {
        paste(clipboard, sheet, dest, undo, formats, dep_graph, workbook);
        return;
    }

    auto entries = (mode == PasteMode::Transpose)
        ? clipboard.paste_at_transposed(dest)
        : clipboard.paste_at(dest);

    for (const auto& pe : entries) {
        if (mode == PasteMode::ValuesOnly) {
            // Always paste as value, ignore formulas
            std::string display = to_display_string(pe.value);
            process(display.c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
        } else if (mode == PasteMode::FormulasOnly) {
            // Only paste cells that have formulas
            if (!pe.formula.empty()) {
                process(("=" + pe.formula).c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
            }
        } else {
            // Transpose: paste both values and formulas
            if (!pe.formula.empty()) {
                process(("=" + pe.formula).c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
            } else {
                std::string display = to_display_string(pe.value);
                process(display.c_str(), sheet, pe.addr, undo, formats, dep_graph, workbook);
            }
        }
    }

    if (clipboard.is_cut()) {
        std::unordered_set<CellAddress> dest_set;
        for (const auto& pe : entries) dest_set.insert(pe.addr);
        for (const auto& src : clipboard.source_cells()) {
            if (!dest_set.contains(src))
                process("", sheet, src, undo, formats, dep_graph, workbook);
        }
        clipboard.clear();
    }
}

}  // namespace magic
