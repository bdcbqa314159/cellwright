#pragma once
#include <deque>
#include <string>

namespace magic {

class ToastManager {
public:
    void show(const std::string& message, float duration_sec = 3.0f);
    void render();
    bool has_pending() const { return !toasts_.empty(); }

private:
    struct Toast {
        std::string message;
        float start_time;
        float duration;
    };
    std::deque<Toast> toasts_;
};

}  // namespace magic
