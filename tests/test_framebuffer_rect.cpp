#include <gtest/gtest.h>
#include "ui/FramebufferRect.hpp"

using namespace magic;

// Reproduces the pre-fix arithmetic so the regression is pinned by a test that
// demonstrably distinguishes the two, rather than only asserting the new values.
static int buggy_y(float win_y_from_top, float win_h, float viewport_h_logical,
                   float scale_y) {
    return static_cast<int>(
        (viewport_h_logical / scale_y - win_y_from_top - win_h) * scale_y);
}

TEST(FramebufferRect, UnscaledDisplayIsUnchanged) {
    // scale == 1: the old code was correct here, which is why the bug shipped.
    const auto r = framebuffer_rect(/*x=*/50, /*y_from_top=*/100, /*w=*/400,
                                    /*h=*/200, /*viewport_h=*/1000,
                                    /*sx=*/1.0f, /*sy=*/1.0f);
    EXPECT_EQ(r.x, 50);
    EXPECT_EQ(r.y, 700);  // 1000 - 100 - 200
    EXPECT_EQ(r.w, 400);
    EXPECT_EQ(r.h, 200);
    EXPECT_EQ(r.y, buggy_y(100, 200, 1000, 1.0f));  // the two agree at scale 1
}

TEST(FramebufferRect, RetinaScaleTwoConvertsLogicalHeightOnce) {
    // The regression. Correct: (1000 - 100 - 200) * 2 = 1400.
    // Pre-fix:  (1000/2 - 100 - 200) * 2 = 400 — off by a full logical height.
    const auto r = framebuffer_rect(50, 100, 400, 200, 1000, 2.0f, 2.0f);
    EXPECT_EQ(r.x, 100);
    EXPECT_EQ(r.y, 1400);
    EXPECT_EQ(r.w, 800);
    EXPECT_EQ(r.h, 400);

    EXPECT_EQ(buggy_y(100, 200, 1000, 2.0f), 400);
    EXPECT_NE(r.y, buggy_y(100, 200, 1000, 2.0f))
        << "test would not detect the regression it exists to catch";
}

TEST(FramebufferRect, FractionalScaleIsHandled) {
    // Windows at 150%.
    const auto r = framebuffer_rect(0, 0, 100, 100, 800, 1.5f, 1.5f);
    EXPECT_EQ(r.y, 1050);  // (800 - 0 - 100) * 1.5
    EXPECT_NE(r.y, buggy_y(0, 100, 800, 1.5f));
}

TEST(FramebufferRect, WindowFlushToBottomGivesZeroY) {
    const auto r = framebuffer_rect(0, 800, 100, 200, 1000, 2.0f, 2.0f);
    EXPECT_EQ(r.y, 0);  // (1000 - 800 - 200) * 2
}

TEST(FramebufferRect, AnisotropicScaleUsesEachAxis) {
    const auto r = framebuffer_rect(10, 20, 30, 40, 500, 2.0f, 3.0f);
    EXPECT_EQ(r.x, 20);
    EXPECT_EQ(r.w, 60);
    EXPECT_EQ(r.y, 1320);  // (500 - 20 - 40) * 3
    EXPECT_EQ(r.h, 120);
}
