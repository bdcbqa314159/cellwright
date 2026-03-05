#include <gtest/gtest.h>
#include "formula/FunctionRegistry.hpp"

using namespace magic;

TEST(FunctionRegistry, UnregisterFunction) {
    FunctionRegistry reg;
    reg.register_function("FOO", [](const std::vector<CellValue>&) { return CellValue{42.0}; });
    EXPECT_TRUE(reg.has("FOO"));
    EXPECT_TRUE(reg.unregister_function("FOO"));
    EXPECT_FALSE(reg.has("FOO"));
}

TEST(FunctionRegistry, UnregisterCaseInsensitive) {
    FunctionRegistry reg;
    reg.register_function("bar", [](const std::vector<CellValue>&) { return CellValue{1.0}; });
    EXPECT_TRUE(reg.unregister_function("Bar"));
    EXPECT_FALSE(reg.has("BAR"));
}

TEST(FunctionRegistry, UnregisterNonexistent) {
    FunctionRegistry reg;
    EXPECT_FALSE(reg.unregister_function("MISSING"));
}

TEST(FunctionRegistry, ReregisterAfterUnregister) {
    FunctionRegistry reg;
    reg.register_function("TEST", [](const std::vector<CellValue>&) { return CellValue{1.0}; });
    (void)reg.unregister_function("TEST");
    reg.register_function("TEST", [](const std::vector<CellValue>&) { return CellValue{2.0}; });
    auto result = reg.call("TEST", {});
    ASSERT_TRUE(is_number(result));
    EXPECT_DOUBLE_EQ(as_number(result), 2.0);
}
