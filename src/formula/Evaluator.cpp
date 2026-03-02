#include "formula/Evaluator.hpp"
#include "core/Sheet.hpp"
#include "core/Workbook.hpp"
#include <cmath>
#include <stdexcept>

namespace magic {

Evaluator::Evaluator(Sheet& sheet, const FunctionRegistry& registry, Workbook* workbook)
    : sheet_(sheet), registry_(registry), workbook_(workbook) {}

CellValue Evaluator::evaluate(const ASTNode& node) {
    return std::visit([this](const auto& v) -> CellValue {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, NumberNode>)      return eval_number(v);
        if constexpr (std::is_same_v<T, StringNode>)      return eval_string(v);
        if constexpr (std::is_same_v<T, BoolNode>)        return eval_bool(v);
        if constexpr (std::is_same_v<T, CellRefNode>)     return eval_cellref(v);
        if constexpr (std::is_same_v<T, RangeNode>)       return eval_range(v);
        if constexpr (std::is_same_v<T, SheetRefNode>)    return eval_sheetref(v);
        if constexpr (std::is_same_v<T, SheetRangeNode>)  return eval_sheetrange(v);
        if constexpr (std::is_same_v<T, UnaryOpNode>)     return eval_unary(v);
        if constexpr (std::is_same_v<T, BinOpNode>)       return eval_binop(v);
        if constexpr (std::is_same_v<T, FuncCallNode>)    return eval_func(v);
        if constexpr (std::is_same_v<T, CompareNode>)     return eval_compare(v);
        return CellValue{CellError::VALUE};
    }, node.value);
}

CellValue Evaluator::eval_number(const NumberNode& n) {
    return CellValue{n.value};
}

CellValue Evaluator::eval_string(const StringNode& n) {
    return CellValue{n.value};
}

CellValue Evaluator::eval_bool(const BoolNode& n) {
    return CellValue{n.value};
}

CellValue Evaluator::eval_cellref(const CellRefNode& n) {
    return sheet_.get_value(n.addr);
}

CellValue Evaluator::eval_range(const RangeNode&) {
    // Standalone range is an error — ranges only valid inside function calls
    return CellValue{CellError::VALUE};
}

CellValue Evaluator::eval_unary(const UnaryOpNode& n) {
    auto val = evaluate(*n.operand);
    if (is_error(val)) return val;
    double d = to_double(val);
    return CellValue{(n.op == '-') ? -d : d};
}

CellValue Evaluator::eval_binop(const BinOpNode& n) {
    auto lv = evaluate(*n.left);
    auto rv = evaluate(*n.right);
    if (is_error(lv)) return lv;
    if (is_error(rv)) return rv;

    double l = to_double(lv);
    double r = to_double(rv);

    switch (n.op) {
        case '+': return CellValue{l + r};
        case '-': return CellValue{l - r};
        case '*': return CellValue{l * r};
        case '/':
            if (r == 0.0) return CellValue{CellError::DIV0};
            return CellValue{l / r};
        case '^': return CellValue{std::pow(l, r)};
        default:  return CellValue{CellError::VALUE};
    }
}

CellValue Evaluator::eval_compare(const CompareNode& n) {
    auto lv = evaluate(*n.left);
    auto rv = evaluate(*n.right);
    if (is_error(lv)) return lv;
    if (is_error(rv)) return rv;

    double l = to_double(lv);
    double r = to_double(rv);

    bool result = false;
    if (n.op == "=")       result = (l == r);
    else if (n.op == "<>") result = (l != r);
    else if (n.op == "<")  result = (l < r);
    else if (n.op == ">")  result = (l > r);
    else if (n.op == "<=") result = (l <= r);
    else if (n.op == ">=") result = (l >= r);

    return CellValue{result};
}

CellValue Evaluator::eval_sheetref(const SheetRefNode& n) {
    Sheet* s = find_sheet(n.sheet_name);
    if (!s) return CellValue{CellError::REF};
    return s->get_value(n.addr);
}

CellValue Evaluator::eval_sheetrange(const SheetRangeNode&) {
    // Standalone sheet range is an error — only valid inside function calls
    return CellValue{CellError::VALUE};
}

std::vector<CellValue> Evaluator::expand_range(const RangeNode& range) {
    return expand_range_on(sheet_, range.from, range.to);
}

std::vector<CellValue> Evaluator::expand_range_on(Sheet& s, const CellAddress& from, const CellAddress& to) {
    std::vector<CellValue> result;
    int32_t c1 = std::min(from.col, to.col);
    int32_t c2 = std::max(from.col, to.col);
    int32_t r1 = std::min(from.row, to.row);
    int32_t r2 = std::max(from.row, to.row);
    result.reserve(static_cast<size_t>(c2 - c1 + 1) * static_cast<size_t>(r2 - r1 + 1));

    for (int32_t c = c1; c <= c2; ++c) {
        for (int32_t r = r1; r <= r2; ++r) {
            result.push_back(s.get_value({c, r}));
        }
    }
    return result;
}

void Evaluator::expand_range_into(Sheet& s, const CellAddress& from, const CellAddress& to, std::vector<CellValue>& out) {
    int32_t c1 = std::min(from.col, to.col);
    int32_t c2 = std::max(from.col, to.col);
    int32_t r1 = std::min(from.row, to.row);
    int32_t r2 = std::max(from.row, to.row);
    out.reserve(out.size() + static_cast<size_t>(c2 - c1 + 1) * static_cast<size_t>(r2 - r1 + 1));

    for (int32_t c = c1; c <= c2; ++c) {
        for (int32_t r = r1; r <= r2; ++r) {
            out.push_back(s.get_value({c, r}));
        }
    }
}

std::vector<CellValue> Evaluator::collect_args(const std::vector<ASTNode*>& args) {
    std::vector<CellValue> flat;
    collect_args_into(args, flat);
    return flat;
}

void Evaluator::collect_args_into(const std::vector<ASTNode*>& args, std::vector<CellValue>& flat) {
    for (const auto& arg : args) {
        if (auto* range = std::get_if<RangeNode>(&arg->value)) {
            expand_range_into(sheet_, range->from, range->to, flat);
        } else if (auto* sr = std::get_if<SheetRangeNode>(&arg->value)) {
            Sheet* s = find_sheet(sr->sheet_name);
            if (!s) {
                flat.push_back(CellValue{CellError::REF});
            } else {
                expand_range_into(*s, sr->from, sr->to, flat);
            }
        } else {
            flat.push_back(evaluate(*arg));
        }
    }
}

CellValue Evaluator::eval_func(const FuncCallNode& n) {
    if (args_depth_ >= args_stack_.size())
        args_stack_.emplace_back();
    auto& buf = args_stack_[args_depth_++];
    buf.clear();
    collect_args_into(n.args, buf);
    auto result = registry_.call_direct(n.name, buf);
    --args_depth_;
    return result;
}

Sheet* Evaluator::find_sheet(const std::string& name) {
    if (!workbook_) return nullptr;
    for (int i = 0; i < workbook_->sheet_count(); ++i) {
        if (workbook_->sheet(i).name() == name)
            return &workbook_->sheet(i);
    }
    return nullptr;
}

}  // namespace magic
