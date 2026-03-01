#pragma once
#include "formula/ASTNode.hpp"
#include "formula/FunctionRegistry.hpp"
#include "core/CellValue.hpp"

namespace magic {

class Sheet;

class Evaluator {
public:
    Evaluator(Sheet& sheet, const FunctionRegistry& registry);

    CellValue evaluate(const ASTNode& node);

private:
    CellValue eval_number(const NumberNode& n);
    CellValue eval_string(const StringNode& n);
    CellValue eval_bool(const BoolNode& n);
    CellValue eval_cellref(const CellRefNode& n);
    CellValue eval_range(const RangeNode& n);      // returns error; ranges handled in func calls
    CellValue eval_unary(const UnaryOpNode& n);
    CellValue eval_binop(const BinOpNode& n);
    CellValue eval_func(const FuncCallNode& n);
    CellValue eval_compare(const CompareNode& n);

    // Expand a range into a flat list of values
    std::vector<CellValue> expand_range(const RangeNode& range);

    // Collect args, expanding ranges
    std::vector<CellValue> collect_args(const std::vector<ASTNodePtr>& args);

    Sheet& sheet_;
    const FunctionRegistry& registry_;
};

}  // namespace magic
