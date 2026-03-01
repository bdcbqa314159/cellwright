#include "formula/AsyncRecalcEngine.hpp"

namespace magic {

AsyncRecalcEngine::AsyncRecalcEngine()
    : worker_([this] { worker_loop(); }) {}

AsyncRecalcEngine::~AsyncRecalcEngine() {
    stop_.store(true);
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

void AsyncRecalcEngine::worker_loop() {
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
}

}  // namespace magic
