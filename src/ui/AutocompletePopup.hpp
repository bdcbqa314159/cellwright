#pragma once
#include <imgui.h>
#include <string>
#include <vector>

namespace magic {

class FunctionRegistry;

class AutocompletePopup {
public:
    // Call each frame while editing. Returns true if user accepted a completion.
    // pos = screen position to anchor the popup below.
    // out_text receives the completed function name + "(" on accept.
    bool render(const char* buf, int cursor_pos, const FunctionRegistry& registry,
                ImVec2 anchor_pos, std::string& out_text);

    void reset();
    bool is_active() const { return active_ && !matches_.empty(); }

    // Returns true if the popup consumed the key event (Up/Down/Tab)
    bool wants_key_input() const { return active_ && !matches_.empty(); }

private:
    bool active_ = false;
    int selected_ = 0;
    std::string prefix_;
    std::vector<std::string> matches_;

    void update_matches(const std::string& prefix, const FunctionRegistry& registry);

public:
    // Extract the current partial token at cursor (for autocomplete prefix).
    static std::string extract_token(const char* buf, int cursor_pos);
    // Find the name of the function whose argument list the cursor is inside.
    // Returns empty if cursor is not inside a function call.
    static std::string find_enclosing_function(const char* buf, int cursor_pos);
};

}  // namespace magic
