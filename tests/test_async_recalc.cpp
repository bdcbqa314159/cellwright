#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

#include "formula/AsyncRecalcEngine.hpp"
using namespace magic;

namespace {

// Poll until `want` results arrive or the deadline passes. The tests previously
// looped a fixed 100 x 10ms, which silently asserts "the worker finishes within
// ~1 second" — a timing property none of them means to test. On a loaded CI runner
// sleep_for(10ms) oversleeps badly, and the budget expired before the worker was
// scheduled. A wall-clock deadline keeps the real assertion (a result arrives)
// without smuggling in a latency one.
std::vector<RecalcResult> wait_for_results(AsyncRecalcEngine& engine, size_t want,
                                           std::chrono::seconds budget = std::chrono::seconds(30)) {
    std::vector<RecalcResult> out;
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (out.size() < want && std::chrono::steady_clock::now() < deadline) {
        auto partial = engine.poll_results();
        out.insert(out.end(), partial.begin(), partial.end());
        if (out.size() >= want) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return out;
}

}  // namespace

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

    const std::vector<RecalcResult> results = wait_for_results(engine, 2);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(results[0].value), 30.0);
    EXPECT_DOUBLE_EQ(std::get<double>(results[1].value), 12.0);
}

TEST(AsyncRecalcEngine, ErrorHandling) {
    AsyncRecalcEngine engine;

    std::vector<RecalcJob> jobs = {{{0, 0}, "bad"}};
    engine.submit(std::move(jobs),
                  [](const std::string&) -> CellValue { throw std::runtime_error("eval failed"); });

    const std::vector<RecalcResult> results = wait_for_results(engine, 1);

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
