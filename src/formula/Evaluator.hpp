#pragma once
#include "formula/ASTNode.hpp"
#include "formula/FunctionRegistry.hpp"
#include "core/CellValue.hpp"

namespace magic {

class Sheet;
class Workbook;

class Evaluator {
public:
    Evaluator(Sheet& sheet, const FunctionRegistry& registry, Workbook* workbook = nullptr);

    CellValue evaluate(const ASTNode& node);

private:
    CellValue eval_number(const NumberNode& n);
    CellValue eval_string(const StringNode& n);
    CellValue eval_bool(const BoolNode& n);
    CellValue eval_cellref(const CellRefNode& n);
    CellValue eval_range(const RangeNode& n);
    CellValue eval_sheetref(const SheetRefNode& n);
    CellValue eval_sheetrange(const SheetRangeNode& n);
    CellValue eval_unary(const UnaryOpNode& n);
    CellValue eval_binop(const BinOpNode& n);
    CellValue eval_func(const FuncCallNode& n);
    CellValue eval_compare(const CompareNode& n);

    std::vector<CellValue> expand_range(const RangeNode& range);
    std::vector<CellValue> expand_range_on(Sheet& s, const CellAddress& from, const CellAddress& to);
    void expand_range_into(Sheet& s, const CellAddress& from, const CellAddress& to, std::vector<CellValue>& out);
    std::vector<CellValue> collect_args(const std::vector<ASTNode*>& args);
    void collect_args_into(const std::vector<ASTNode*>& args, std::vector<CellValue>& out);

    Sheet* find_sheet(const std::string& name);

    Sheet& sheet_;
    const FunctionRegistry& registry_;
    Workbook* workbook_;

    // Scratch buffers reused per recursion depth to avoid repeated allocation
    std::vector<std::vector<CellValue>> args_pool_;
    size_t pool_depth_ = 0;
};

}  // namespace magic
