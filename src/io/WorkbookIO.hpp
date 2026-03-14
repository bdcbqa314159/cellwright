#pragma once
#include "core/Workbook.hpp"
#include <filesystem>
#include <string>

namespace magic {

// JSON-based workbook serialization (.magic files)
class WorkbookIO {
public:
    [[nodiscard]] static bool save(const std::filesystem::path& path, const Workbook& workbook);
    [[nodiscard]] static bool load(const std::filesystem::path& path, Workbook& workbook);

    static std::string to_json(const Workbook& workbook);
    [[nodiscard]] static bool from_json(const std::string& json, Workbook& workbook);
};

}  // namespace magic
