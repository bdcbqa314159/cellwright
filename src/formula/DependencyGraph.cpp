#include "formula/DependencyGraph.hpp"
#include <algorithm>
#include <optional>
#include <queue>

namespace magic {

void DependencyGraph::set_dependencies(const CellAddress& cell, const std::vector<CellAddress>& new_deps) {
    // Remove old reverse edges
    if (auto it = forward_deps_.find(cell); it != forward_deps_.end()) {
        for (const auto& old_dep : it->second) {
            if (auto rit = reverse_deps_.find(old_dep); rit != reverse_deps_.end()) {
                rit->second.erase(cell);
            }
        }
    }

    forward_deps_[cell] = new_deps;

    // Add new reverse edges
    for (const auto& dep : new_deps) {
        reverse_deps_[dep].insert(cell);
    }
}

void DependencyGraph::remove(const CellAddress& cell) {
    // Remove forward edges (what cell depends on)
    if (auto it = forward_deps_.find(cell); it != forward_deps_.end()) {
        for (const auto& dep : it->second) {
            if (auto rit = reverse_deps_.find(dep); rit != reverse_deps_.end()) {
                rit->second.erase(cell);
            }
        }
        forward_deps_.erase(it);
    }

    // Remove reverse edges: clean up deps of cells that depend on this cell
    if (auto rit = reverse_deps_.find(cell); rit != reverse_deps_.end()) {
        for (const auto& dependent : rit->second) {
            if (auto dit = forward_deps_.find(dependent); dit != forward_deps_.end()) {
                auto& dv = dit->second;
                dv.erase(std::remove(dv.begin(), dv.end(), cell), dv.end());
            }
        }
        reverse_deps_.erase(rit);
    }
}

std::vector<CellAddress> DependencyGraph::dependents_of(const CellAddress& cell) const {
    auto it = reverse_deps_.find(cell);
    if (it == reverse_deps_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

RecalcPlan DependencyGraph::recalc_order(const std::unordered_set<CellAddress>& changed) const {
    // BFS to find all transitively affected cells
    std::unordered_set<CellAddress> affected;
    std::queue<CellAddress> q;

    for (const auto& cell : changed) {
        q.push(cell);
        affected.insert(cell);
    }

    while (!q.empty()) {
        auto curr = q.front();
        q.pop();
        auto it = reverse_deps_.find(curr);
        if (it == reverse_deps_.end()) continue;
        for (const auto& dep : it->second) {
            if (affected.insert(dep).second) {
                q.push(dep);
            }
        }
    }

    // Topological sort via Kahn's algorithm over affected cells
    std::unordered_map<CellAddress, int> in_degree;
    for (const auto& cell : affected) {
        if (in_degree.find(cell) == in_degree.end()) in_degree[cell] = 0;
        auto it = reverse_deps_.find(cell);
        if (it == reverse_deps_.end()) continue;
        for (const auto& dep : it->second) {
            if (affected.count(dep)) {
                in_degree[dep]++;
            }
        }
    }

    std::queue<CellAddress> ready;
    for (const auto& [cell, deg] : in_degree) {
        if (deg == 0) ready.push(cell);
    }

    RecalcPlan plan;
    plan.order.reserve(affected.size());
    while (!ready.empty()) {
        auto curr = ready.front();
        ready.pop();
        plan.order.push_back(curr);

        auto it = reverse_deps_.find(curr);
        if (it == reverse_deps_.end()) continue;
        for (const auto& dep : it->second) {
            if (affected.count(dep)) {
                if (--in_degree[dep] == 0) {
                    ready.push(dep);
                }
            }
        }
    }

    // Detect circular dependencies: cells with non-zero in-degree are in cycles.
    for (const auto& [cell, deg] : in_degree) {
        if (deg > 0) {
            plan.cycles.insert(cell);
        }
    }

    return plan;
}

void DependencyGraph::clear() {
    forward_deps_.clear();
    reverse_deps_.clear();
}

// Returns shifted address, or nullopt if the cell was at the deleted position
static std::optional<CellAddress> shift_addr(const CellAddress& addr, bool is_row, int32_t at, int32_t delta) {
    CellAddress result = addr;
    int32_t& coord = is_row ? result.row : result.col;
    if (delta < 0 && coord == at) return std::nullopt;  // deleted
    if (coord >= at) coord += delta;
    return result;
}

static void shift_impl(
    std::unordered_map<CellAddress, std::vector<CellAddress>>& forward,
    std::unordered_map<CellAddress, std::unordered_set<CellAddress>>& reverse,
    bool is_row, int32_t at, int32_t delta)
{
    // Rebuild both maps with shifted addresses, dropping deleted entries
    std::unordered_map<CellAddress, std::vector<CellAddress>> new_forward;
    for (auto& [cell, deps] : forward) {
        auto new_cell = shift_addr(cell, is_row, at, delta);
        if (!new_cell) continue;  // cell was at deleted position
        auto& new_deps = new_forward[*new_cell];
        for (auto& dep : deps) {
            auto new_dep = shift_addr(dep, is_row, at, delta);
            if (new_dep) new_deps.push_back(*new_dep);
        }
    }
    forward = std::move(new_forward);

    // Rebuild reverse from forward
    reverse.clear();
    for (auto& [cell, deps] : forward) {
        for (auto& dep : deps) {
            reverse[dep].insert(cell);
        }
    }
}

void DependencyGraph::shift_rows(int32_t at, int32_t delta) {
    shift_impl(forward_deps_, reverse_deps_, true, at, delta);
}

void DependencyGraph::shift_cols(int32_t at, int32_t delta) {
    shift_impl(forward_deps_, reverse_deps_, false, at, delta);
}

}  // namespace magic
