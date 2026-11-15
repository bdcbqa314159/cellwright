#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"

namespace magic {

// A unit of recalculation work: a cell address + its formula
struct RecalcJob {
    CellAddress addr;
    std::string formula;
};

// Result of evaluating one formula
struct RecalcResult {
    CellAddress addr;
    CellValue value;
};

// Evaluator callback: given a formula, produce a CellValue.
// This is called on the worker thread — the caller must ensure thread-safety
// of any data the callback accesses.
using EvalFunc = std::function<CellValue(const std::string& formula)>;

class AsyncRecalcEngine {
public:
    AsyncRecalcEngine();
    ~AsyncRecalcEngine();

    // Submit a batch of cells for background evaluation.
    // eval_fn is called on the worker thread for each job.
    void submit(std::vector<RecalcJob> jobs, EvalFunc eval_fn);

    // Poll for completed results (call from main thread each frame).
    // Returns completed results and clears the internal buffer.
    std::vector<RecalcResult> poll_results();

    // True if the engine is processing or has pending batches.
    bool is_busy() const { return busy_.load() || has_pending(); }

private:
    void worker_loop();
    bool has_pending() const {
        std::lock_guard lock(queue_mutex_);
        return !pending_.empty();
    }

    struct Batch {
        std::vector<RecalcJob> jobs;
        EvalFunc eval_fn;
    };

    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<Batch> pending_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> busy_{false};

    std::mutex result_mutex_;
    std::vector<RecalcResult> results_;

    // DECLARED LAST, DELIBERATELY. Members are initialised in declaration order, so
    // anything declared after this would still be unconstructed while worker_loop()
    // is already touching it. The thread is also started in the constructor BODY
    // rather than the init list, so the ordering does not depend on this line alone.
    std::thread worker_;
};

}  // namespace magic
