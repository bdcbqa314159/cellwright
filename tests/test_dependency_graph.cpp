#include <gtest/gtest.h>
#include "formula/DependencyGraph.hpp"

using namespace magic;

TEST(DependencyGraph, SetDependencies) {
    DependencyGraph graph;
    CellAddress a1{0, 0};
    CellAddress b1{1, 0};
    CellAddress c1{2, 0};

    // C1 depends on A1 and B1
    graph.set_dependencies(c1, {a1, b1});

    auto deps = graph.dependents_of(a1);
    ASSERT_EQ(deps.size(), 1u);
    EXPECT_EQ(deps[0], c1);
}

TEST(DependencyGraph, RecalcOrder) {
    DependencyGraph graph;
    CellAddress a1{0, 0};
    CellAddress b1{1, 0};
    CellAddress c1{2, 0};

    // B1 depends on A1, C1 depends on B1
    graph.set_dependencies(b1, {a1});
    graph.set_dependencies(c1, {b1});

    std::unordered_set<CellAddress> changed = {a1};
    auto plan = graph.recalc_order(changed);

    // Should contain A1, B1, C1 in topological order with no cycles
    ASSERT_EQ(plan.order.size(), 3u);
    EXPECT_TRUE(plan.cycles.empty());

    // A1 should come before B1, B1 before C1
    int a1_pos = -1, b1_pos = -1, c1_pos = -1;
    for (int i = 0; i < static_cast<int>(plan.order.size()); ++i) {
        if (plan.order[i] == a1) a1_pos = i;
        if (plan.order[i] == b1) b1_pos = i;
        if (plan.order[i] == c1) c1_pos = i;
    }
    EXPECT_LT(a1_pos, b1_pos);
    EXPECT_LT(b1_pos, c1_pos);
}

TEST(DependencyGraph, Remove) {
    DependencyGraph graph;
    CellAddress a1{0, 0};
    CellAddress b1{1, 0};

    graph.set_dependencies(b1, {a1});
    graph.remove(b1);

    auto deps = graph.dependents_of(a1);
    EXPECT_TRUE(deps.empty());
}
