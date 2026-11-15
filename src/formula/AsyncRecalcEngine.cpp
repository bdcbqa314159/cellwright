#include "formula/AsyncRecalcEngine.hpp"
#include <mutex>

namespace magic {

AsyncRecalcEngine::AsyncRecalcEngine()
    : worker_([this] { worker_loop(); }) {}

AsyncRecalcEngine::~AsyncRecalcEngine() {
    // stop_ is part of the condition variable's predicate, so it must be changed
    // under queue_mutex_. Setting it without the lock loses the wakeup: the worker
    // evaluates the predicate while holding the lock, and a notify_one issued
    // before it has actually blocked is dropped — after which join() waits forever.
    // stop_ being atomic prevents a torn read, not a lost notification.
    {
        std::lock_guard lock(queue_mutex_);
        stop_.store(true);
    }
    queue_cv_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void AsyncRecalcEngine::submit(std::vector<RecalcJob> jobs, EvalFunc eval_fn) {
    {
        std::lock_guard lock(queue_mutex_);
        pending_.push({std::move(jobs), std::move(eval_fn)});
    }
    queue_cv_.notify_one();
}

std::vector<RecalcResult> AsyncRecalcEngine::poll_results() {
    std::lock_guard lock(result_mutex_);
    std::vector<RecalcResult> out;
    out.swap(results_);
    return out;
}

void AsyncRecalcEngine::worker_loop() try {
    while (true) {
        Batch batch;
        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return stop_.load() || !pending_.empty(); });
            if (stop_.load() && pending_.empty()) return;
            batch = std::move(pending_.front());
            pending_.pop();
        }

        busy_.store(true);

        std::vector<RecalcResult> batch_results;
        batch_results.reserve(batch.jobs.size());

        for (const auto& job : batch.jobs) {
            if (stop_.load()) break;
            CellValue val;
            try {
                val = batch.eval_fn(job.formula);
            } catch (...) {
                val = CellValue{CellError::VALUE};
            }
            batch_results.push_back({job.addr, std::move(val)});
        }

        {
            std::lock_guard lock(result_mutex_);
            results_.insert(results_.end(),
                           std::make_move_iterator(batch_results.begin()),
                           std::make_move_iterator(batch_results.end()));
        }

        busy_.store(false);
    }
} catch (...) {
    // A thread function must not let an exception escape: that calls std::terminate
    // and aborts the process. The per-job try below covers eval_fn, but the queue and
    // result containers can also throw (bad_alloc). Mark the engine idle and stop,
    // so a waiting destructor still joins instead of hanging.
    busy_.store(false);
    stop_.store(true);
}

}  // namespace magic
