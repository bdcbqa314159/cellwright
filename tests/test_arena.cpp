#include <gtest/gtest.h>
#include "core/Arena.hpp"
#include <string>
#include <vector>

using namespace magic;

TEST(Arena, BasicAllocate) {
    Arena arena(256);
    auto* p = static_cast<int*>(arena.allocate(sizeof(int), alignof(int)));
    ASSERT_NE(p, nullptr);
    *p = 42;
    EXPECT_EQ(*p, 42);
    EXPECT_GE(arena.bytes_used(), sizeof(int));
}

TEST(Arena, CreateTrivial) {
    Arena arena(256);
    auto* val = arena.create<double>(3.14);
    EXPECT_DOUBLE_EQ(*val, 3.14);
}

TEST(Arena, CreateWithDestructor) {
    static int dtor_count = 0;
    struct Tracked {
        ~Tracked() { ++dtor_count; }
    };

    dtor_count = 0;
    {
        Arena arena(256);
        arena.create<Tracked>();
        arena.create<Tracked>();
        arena.create<Tracked>();
        EXPECT_EQ(dtor_count, 0);
    }
    EXPECT_EQ(dtor_count, 3);
}

TEST(Arena, DestructorReverseOrder) {
    static std::vector<int> order;
    struct Tracked {
        int id;
        explicit Tracked(int i) : id(i) {}
        ~Tracked() { order.push_back(id); }
    };

    order.clear();
    {
        Arena arena(256);
        arena.create<Tracked>(1);
        arena.create<Tracked>(2);
        arena.create<Tracked>(3);
    }
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 1);
}

TEST(Arena, ResetKeepsMemory) {
    Arena arena(256);
    arena.create<std::string>("hello world");
    size_t before = arena.bytes_used();
    EXPECT_GT(before, 0u);

    arena.reset();
    EXPECT_EQ(arena.bytes_used(), 0u);

    // Can still allocate after reset
    arena.create<std::string>("after reset");
    EXPECT_GT(arena.bytes_used(), 0u);
}

TEST(Arena, LargeAllocationGrows) {
    Arena arena(64);
    // Allocate more than the initial block
    auto* p = static_cast<char*>(arena.allocate(128));
    ASSERT_NE(p, nullptr);
    EXPECT_GE(arena.bytes_used(), 128u);
}

TEST(Arena, MoveConstruct) {
    Arena a(256);
    a.create<std::string>("test");

    Arena b(std::move(a));
    EXPECT_GT(b.bytes_used(), 0u);
}

TEST(Arena, ManySmallAllocations) {
    Arena arena(128);
    for (int i = 0; i < 1000; ++i) {
        auto* p = arena.create<int>(i);
        EXPECT_EQ(*p, i);
    }
}
