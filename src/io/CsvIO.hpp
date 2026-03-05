#pragma once
#include "core/Sheet.hpp"
#include <string>
#include <filesystem>

namespace magic {

class CsvIO {
public:
    // Import CSV into a sheet, replacing its contents
    static bool import_file(const std::filesystem::path& path, Sheet& sheet);

    // Export a sheet to CSV
    static bool export_file(const std::filesystem::path& path, const Sheet& sheet);

    // Parse CSV string into a sheet
    static void parse(const std::string& csv, Sheet& sheet);

    // Serialize a sheet to CSV string
    static std::string serialize(const Sheet& sheet);

private:
    // Parse a single CSV line respecting quoted fields
    static std::vector<std::string> parse_csv_line(const std::string& line);
};

}  // namespace magic
