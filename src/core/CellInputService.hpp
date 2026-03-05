#pragma once
#include "core/CellAddress.hpp"
#include <unordered_set>

namespace magic {

class Sheet;
class Workbook;
class FunctionRegistry;
class DependencyGraph;
class Clipboard;
class FormatMap;
class UndoManager;

class CellInputService {
public:
    explicit CellInputService(FunctionRegistry& registry);

    // Process cell input (formula or value), updating deps, undo, and recalcing dependents.
    void process(const char* buf, Sheet& sheet, const CellAddress& addr,
                 UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                 Workbook& workbook);

    // Like process() but skips recalc_dependents (for batch use).
    void process_no_recalc(const char* buf, Sheet& sheet, const CellAddress& addr,
                           UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
                           Workbook& workbook);

    // Batch recalc: given changed cells, do one topological sort and re-evaluate.
    void batch_recalc(Sheet& sheet, const std::unordered_set<CellAddress>& changed,
                      DependencyGraph& dep_graph, Workbook* workbook = nullptr);

    // Recalc cells that depend on addr.
    void recalc_dependents(Sheet& sheet, const CellAddress& addr,
                           DependencyGraph& dep_graph, Workbook* workbook = nullptr);

    // Clipboard operations (consolidate duplicated menu/keyboard logic).
    // selected: current cell; range_min/max: range bounds if has_range is true.
    void copy(Clipboard& clipboard, const Sheet& sheet,
              const CellAddress& selected, bool has_range,
              const CellAddress& range_min, const CellAddress& range_max);
    void cut(Clipboard& clipboard, const Sheet& sheet,
             const CellAddress& selected, bool has_range,
             const CellAddress& range_min, const CellAddress& range_max);
    void paste(Clipboard& clipboard, Sheet& sheet, const CellAddress& dest,
               UndoManager& undo, FormatMap& formats, DependencyGraph& dep_graph,
               Workbook& workbook);

private:
    FunctionRegistry& registry_;
};

}  // namespace magic
