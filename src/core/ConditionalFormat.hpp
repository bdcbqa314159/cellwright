#pragma once
#include <cstdint>
#include <vector>

namespace magic {

enum class ConditionOp {
    GreaterThan,
    LessThan,
    GreaterEqual,
    LessEqual,
    Equal,
    NotEqual,
};

struct CondColor {
    uint8_t r = 255, g = 200, b = 200, a = 80;  // default: light red tint
};

struct ConditionalRule {
    int32_t column = 0;       // which column this rule applies to (-1 = all)
    ConditionOp op = ConditionOp::GreaterThan;
    double threshold = 0.0;
    CondColor color;
};

class ConditionalFormatStore {
public:
    void add_rule(const ConditionalRule& rule) { rules_.push_back(rule); }
    void remove_rule(std::size_t index) {
        if (index < rules_.size())
            rules_.erase(rules_.begin() + static_cast<ptrdiff_t>(index));
    }
    void clear() { rules_.clear(); }

    [[nodiscard]] const std::vector<ConditionalRule>& rules() const { return rules_; }
    [[nodiscard]] std::vector<ConditionalRule>& rules() { return rules_; }
    [[nodiscard]] std::size_t size() const { return rules_.size(); }

    // Evaluate all rules for a cell. Returns the color of the first matching
    // rule, or nullptr if no rule matches.
    [[nodiscard]] const CondColor* evaluate(int32_t col, double value) const {
        for (const auto& rule : rules_) {
            if (rule.column != -1 && rule.column != col) continue;
            bool match = false;
            switch (rule.op) {
                case ConditionOp::GreaterThan:  match = value > rule.threshold; break;
                case ConditionOp::LessThan:     match = value < rule.threshold; break;
                case ConditionOp::GreaterEqual: match = value >= rule.threshold; break;
                case ConditionOp::LessEqual:    match = value <= rule.threshold; break;
                case ConditionOp::Equal:        match = value == rule.threshold; break;
                case ConditionOp::NotEqual:     match = value != rule.threshold; break;
            }
            if (match) return &rule.color;
        }
        return nullptr;
    }

private:
    std::vector<ConditionalRule> rules_;
};

}  // namespace magic
