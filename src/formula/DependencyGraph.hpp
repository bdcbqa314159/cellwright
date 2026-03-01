#pragma once
#include "core/CellAddress.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace magic {

class DependencyGraph {
public:
    // Set the dependencies for a cell (what cells it references)
    void set_dependencies(const CellAddress& cell, const std::vector<CellAddress>& deps);

    // Remove a cell and all its edges
    void remove(const CellAddress& cell);

    // Get cells that depend on the given cell (direct dependents)
    std::vector<CellAddress> dependents_of(const CellAddress& cell) const;

    // Given a set of changed cells, return all cells that need recalculation
    // in topological order (dependencies first)
    std::vector<CellAddress> recalc_order(const std::unordered_set<CellAddress>& changed) const;

    void clear();

private:
    // cell → cells it references
    std::unordered_map<CellAddress, std::vector<CellAddress>> deps_;
    // cell → cells that reference it (reverse edges)
    std::unordered_map<CellAddress, std::unordered_set<CellAddress>> rdeps_;
};

}  // namespace magic
