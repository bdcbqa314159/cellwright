#include <gtest/gtest.h>
#include "core/Arena.hpp"
#include <string>
#include <vector>

#include <cstdint>
#include <cstdlib>
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

// ── bytes_used() accounting ──────────────────────────────────────────────────
//
// The pre-fix implementation assumed "all blocks except the last are fully used"
// and added their full capacity. That is wrong in two ways: a block abandoned by
// growth is not full, and after reset() nothing is used at all. Sizes below are
// chosen so blocks do NOT fill exactly — with exact fills the old arithmetic
// happens to agree, which is why ResetKeepsMemory passed while the bug was live.

TEST(Arena, BytesUsedIgnoresAbandonedBlockPadding) {
    Arena arena(100);
    arena.allocate(60, 1);  // block 0: offset 60, 40 bytes left
    arena.allocate(60, 1);  // does not fit -> new block, offset 60

    // Actually consumed: 60 + 60. The old code reported capacity(100) + 60 = 160.
    EXPECT_EQ(arena.bytes_used(), 120u);
}

TEST(Arena, BytesUsedIsZeroAfterResetAcrossManyBlocks) {
    Arena arena(64);
    for (int i = 0; i < 10; ++i) arena.allocate(64, 1);  // forces several blocks
    EXPECT_GT(arena.bytes_used(), 0u);

    arena.reset();
    // Old code returned the summed capacity of every non-last block (448 observed).
    EXPECT_EQ(arena.bytes_used(), 0u);
}

TEST(Arena, BytesUsedTracksReuseAfterReset) {
    Arena arena(64);
    for (int i = 0; i < 10; ++i) arena.allocate(64, 1);
    arena.reset();
    arena.allocate(32, 1);
    EXPECT_EQ(arena.bytes_used(), 32u);
}

// ── over-alignment ───────────────────────────────────────────────────────────
//
// allocate() aligned the OFFSET, not the absolute address. That coincides only
// while the block start is at least as aligned as the request; malloc guarantees
// alignof(max_align_t) (16 here), so any request above that could return
// misaligned memory. Measured 3/200 misaligned at align=64 before the fix.

TEST(Arena, OverAlignedAllocationIsActuallyAligned) {
    for (size_t align : {size_t{32}, size_t{64}, size_t{128}}) {
        for (int trial = 0; trial < 64; ++trial) {
            // Fresh arenas with churn so block start addresses vary.
            void* churn = std::malloc(24 + static_cast<size_t>(trial % 7) * 8);
            Arena arena(96);
            void* p = arena.allocate(8, align);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % align, 0u)
                << "align=" << align << " trial=" << trial;
            std::free(churn);
        }
    }
}

TEST(Arena, AlignmentHonouredWhenAllocationForcesGrowth) {
    Arena arena(64);
    arena.allocate(60, 1);                       // nearly fill the block
    void* p = arena.allocate(8, 64);             // must grow, and still be aligned
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64u, 0u);
}

TEST(Arena, SuccessiveOverAlignedAllocationsStayAligned) {
    Arena arena(4096);
    for (int i = 0; i < 16; ++i) {
        void* p = arena.allocate(8, 64);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64u, 0u) << "i=" << i;
    }
}

TEST(Arena, BytesUsedCountsAlignmentPadding) {
    Arena arena(4096);
    arena.allocate(1, 1);          // offset 1
    arena.allocate(8, 64);         // pads up to the next 64-boundary
    // Padding is consumed space, so it must be reflected rather than lost.
    EXPECT_GE(arena.bytes_used(), 9u);
    EXPECT_LE(arena.bytes_used(), 64u + 8u);
}
