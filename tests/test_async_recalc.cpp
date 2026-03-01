#include <gtest/gtest.h>
#include "formula/AsyncRecalcEngine.hpp"
#include <thread>
#include <chrono>

using namespace magic;

TEST(AsyncRecalcEngine, BasicBatch) {
    AsyncRecalcEngine engine;

    std::vector<RecalcJob> jobs = {
        {{0, 0}, "10+20"},
        {{1, 0}, "3*4"},
    };

    // Simple eval_fn that just returns fixed values based on formula
    engine.submit(std::move(jobs), [](const std::string& formula) -> CellValue {
        if (formula == "10+20") return CellValue{30.0};
        if (formula == "3*4") return CellValue{12.0};
        return CellValue{CellError::VALUE};
    });

    // Wait for results
    std::vector<RecalcResult> results;
    for (int i = 0; i < 100 && results.size() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto partial = engine.poll_results();
        results.insert(results.end(), partial.begin(), partial.end());
    }

    ASSERT_EQ(results.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(results[0].value), 30.0);
    EXPECT_DOUBLE_EQ(std::get<double>(results[1].value), 12.0);
}

TEST(AsyncRecalcEngine, ErrorHandling) {
    AsyncRecalcEngine engine;

    std::vector<RecalcJob> jobs = {{{0, 0}, "bad"}};
    engine.submit(std::move(jobs), [](const std::string&) -> CellValue {
        throw std::runtime_error("eval failed");
    });

    std::vector<RecalcResult> results;
    for (int i = 0; i < 100 && results.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        results = engine.poll_results();
    }

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<CellError>(results[0].value));
    EXPECT_EQ(std::get<CellError>(results[0].value), CellError::VALUE);
}
