#include <gtest/gtest.h>
#include "formula/AsyncRecalcEngine.hpp"
#include <thread>
#include <chrono>

#include <stdexcept>
#include <variant>
#include <vector>
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

// ── Shutdown path ───────────────────────────────────────────────────────────
//
// These exercise the destructor-vs-worker interleaving that made BasicBatch
// intermittently abort on CI (issue #11). They are NOT reliable red-first tests:
// the bug was a lost wakeup whose window is sub-microsecond, and it never
// reproduced on a fast uncontended machine (0/60 isolated, 0/10 full-suite).
// What they do is drive the exact path many times, so a regression that widens
// the window again shows up as a hang (caught by the ctest TIMEOUT property) or
// an abort rather than passing silently.

TEST(AsyncRecalcEngine, DestroyIdleEngineRepeatedly) {
    // The pure lost-wakeup window: the worker is parked in wait() and the
    // destructor must both change the predicate and wake it.
    for (int i = 0; i < 200; ++i) {
        AsyncRecalcEngine engine;
    }
}

TEST(AsyncRecalcEngine, DestroyWithWorkInFlightRepeatedly) {
    // Destructor races the worker mid-batch: it may be waiting, dequeuing, or
    // part-way through the job loop when stop_ is set.
    for (int i = 0; i < 200; ++i) {
        AsyncRecalcEngine engine;
        std::vector<RecalcJob> jobs;
        for (int j = 0; j < 8; ++j) jobs.push_back({{j, 0}, "1+1"});
        engine.submit(std::move(jobs),
                      [](const std::string&) -> CellValue { return CellValue{2.0}; });
        // Deliberately no poll and no wait — destroy immediately.
    }
}

TEST(AsyncRecalcEngine, ThrowingEvalFuncDoesNotAbortTheProcess) {
    // An exception escaping the worker thread would reach std::terminate. eval_fn
    // is guarded, so this must be reported as a value rather than killing us.
    AsyncRecalcEngine engine;
    std::vector<RecalcJob> jobs = {{{0, 0}, "boom"}};
    engine.submit(std::move(jobs), [](const std::string&) -> CellValue {
        throw std::runtime_error("eval exploded");
    });

    std::vector<RecalcResult> results;
    for (int i = 0; i < 100 && results.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto partial = engine.poll_results();
        results.insert(results.end(), partial.begin(), partial.end());
    }
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<CellError>(results[0].value));
}
