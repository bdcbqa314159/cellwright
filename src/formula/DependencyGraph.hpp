#pragma once
#include "core/CellAddress.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace magic {

// Result of recalc_order(): separated topological order and cycle cells.
struct RecalcPlan {
    std::vector<CellAddress> order;                   // valid topological recalc order
    std::unordered_set<CellAddress> cycles;           // cells involved in circular deps
};

class DependencyGraph {
public:
    // Set the dependencies for a cell (what cells it references)
    void set_dependencies(const CellAddress& cell, const std::vector<CellAddress>& deps);

    // Remove a cell and all its edges
    void remove(const CellAddress& cell);

    // Get cells that depend on the given cell (direct dependents)
    std::vector<CellAddress> dependents_of(const CellAddress& cell) const;

    // Given a set of changed cells, return a RecalcPlan with cells in
    // topological order (dependencies first) and any cycle cells separated.
    RecalcPlan recalc_order(const std::unordered_set<CellAddress>& changed) const;

    void clear();

private:
    // cell → cells it references
    std::unordered_map<CellAddress, std::vector<CellAddress>> forward_deps_;
    // cell → cells that reference it (reverse edges)
    std::unordered_map<CellAddress, std::unordered_set<CellAddress>> reverse_deps_;
};

}  // namespace magic
