#pragma once
#include "core/CellAddress.hpp"
#include <string>

namespace magic {

class Sheet;

class CellEditor {
public:
    bool is_editing() const { return editing_; }
    const CellAddress& editing_cell() const { return cell_; }

    void begin_edit(const CellAddress& cell, const std::string& initial);
    bool render();  // returns true on commit
    void cancel();

    const char* buffer() const { return buf_; }

private:
    bool editing_ = false;
    CellAddress cell_;
    char buf_[1024] = {};
};

}  // namespace magic
