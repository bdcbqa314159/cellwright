#pragma once
#include "core/CellAddress.hpp"
#include <string>
#include <vector>

namespace magic {

class Sheet;

class FindBar {
public:
    void render(Sheet& sheet);

    bool is_visible() const { return visible_; }
    void show_find() { visible_ = true; show_replace_ = false; }
    void show_replace() { visible_ = true; show_replace_ = true; }
    void hide() { visible_ = false; matches_.clear(); }

    // Navigation target after find (consumed by MainWindow)
    bool has_nav_target() const { return has_nav_; }
    CellAddress consume_nav_target() { has_nav_ = false; return nav_target_; }

    // Match list for highlight rendering
    const std::vector<CellAddress>& matches() const { return matches_; }
    int current_match_index() const { return match_idx_; }

private:
    void do_search(Sheet& sheet);
    void find_next();
    void find_prev();

    bool visible_ = false;
    bool show_replace_ = false;
    char search_buf_[256] = {};
    char replace_buf_[256] = {};
    std::vector<CellAddress> matches_;
    int match_idx_ = -1;
    bool has_nav_ = false;
    CellAddress nav_target_{0, 0};
    std::string last_search_;
};

}  // namespace magic
