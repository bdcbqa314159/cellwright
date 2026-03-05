#pragma once
#include <deque>
#include <string>

namespace magic {

class ToastManager {
public:
    void show(const std::string& message, double duration_sec = 3.0);
    void render();
    bool has_pending() const { return !toasts_.empty(); }

private:
    struct Toast {
        std::string message;
        double start_time;
        double duration;
    };
    std::deque<Toast> toasts_;
};

}  // namespace magic
