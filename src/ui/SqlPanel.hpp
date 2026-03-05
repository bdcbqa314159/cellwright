#pragma once
#include "core/DuckDBEngine.hpp"

namespace magic {

class Sheet;

class SqlPanel {
public:
    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }
    void render(DuckDBEngine& engine, Sheet& sheet);

private:
    bool visible_ = false;
    char sql_buf_[4096] = "SELECT * FROM data LIMIT 10";
    QueryResult last_result_;
};

}  // namespace magic
