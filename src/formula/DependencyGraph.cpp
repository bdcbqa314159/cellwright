#include "formula/DependencyGraph.hpp"
#include <algorithm>
#include <queue>
#include <stdexcept>

namespace magic {

void DependencyGraph::set_dependencies(const CellAddress& cell, const std::vector<CellAddress>& new_deps) {
    // Remove old reverse edges
    if (auto it = deps_.find(cell); it != deps_.end()) {
        for (const auto& old_dep : it->second) {
            if (auto rit = rdeps_.find(old_dep); rit != rdeps_.end()) {
                rit->second.erase(cell);
            }
        }
    }

    deps_[cell] = new_deps;

    // Add new reverse edges
    for (const auto& dep : new_deps) {
        rdeps_[dep].insert(cell);
    }
}

void DependencyGraph::remove(const CellAddress& cell) {
    // Remove forward edges
    if (auto it = deps_.find(cell); it != deps_.end()) {
        for (const auto& dep : it->second) {
            if (auto rit = rdeps_.find(dep); rit != rdeps_.end()) {
                rit->second.erase(cell);
            }
        }
        deps_.erase(it);
    }

    // Remove reverse edges where this cell is a dependency
    if (auto rit = rdeps_.find(cell); rit != rdeps_.end()) {
        rdeps_.erase(rit);
    }
}

std::vector<CellAddress> DependencyGraph::dependents_of(const CellAddress& cell) const {
    auto it = rdeps_.find(cell);
    if (it == rdeps_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

std::vector<CellAddress> DependencyGraph::recalc_order(const std::unordered_set<CellAddress>& changed) const {
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
        auto it = rdeps_.find(curr);
        if (it == rdeps_.end()) continue;
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
        auto it = rdeps_.find(cell);
        if (it == rdeps_.end()) continue;
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

    std::vector<CellAddress> order;
    while (!ready.empty()) {
        auto curr = ready.front();
        ready.pop();
        order.push_back(curr);

        auto it = rdeps_.find(curr);
        if (it == rdeps_.end()) continue;
        for (const auto& dep : it->second) {
            if (affected.count(dep)) {
                if (--in_degree[dep] == 0) {
                    ready.push(dep);
                }
            }
        }
    }

    return order;
}

void DependencyGraph::clear() {
    deps_.clear();
    rdeps_.clear();
}

}  // namespace magic
