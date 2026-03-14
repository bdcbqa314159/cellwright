#include "io/CsvIO.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace magic {

std::vector<std::string> CsvIO::parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;  // skip escaped quote
                } else {
                    in_quotes = false;
                }
            } else {
                field += ch;
            }
        } else {
            if (ch == '"') {
                in_quotes = true;
            } else if (ch == ',') {
                fields.push_back(field);
                field.clear();
            } else {
                field += ch;
            }
        }
    }
    fields.push_back(field);
    return fields;
}

void CsvIO::parse(const std::string& csv, Sheet& sheet) {
    static constexpr int32_t MAX_CSV_ROWS = 1048576;
    static constexpr int32_t MAX_CSV_COLS = 16384;

    std::istringstream stream(csv);
    std::string line;
    int32_t row = 0;

    while (std::getline(stream, line)) {
        if (row >= MAX_CSV_ROWS) break;

        // Strip trailing \r for Windows line endings
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() && stream.eof()) break;

        auto fields = parse_csv_line(line);
        int32_t col_limit = std::min(static_cast<int32_t>(fields.size()), MAX_CSV_COLS);
        for (int32_t col = 0; col < col_limit; ++col) {
            const auto& f = fields[col];
            if (f.empty()) continue;

            // Try number first
            try {
                size_t pos = 0;
                double d = std::stod(f, &pos);
                if (pos == f.size()) {
                    sheet.set_value({col, row}, CellValue{d});
                    continue;
                }
            } catch (...) {}

            sheet.set_value({col, row}, CellValue{f});
        }
        ++row;
    }
}

std::string CsvIO::serialize(const Sheet& sheet) {
    std::ostringstream out;
    int32_t max_row = sheet.row_count();
    int32_t max_col = sheet.col_count();

    // Find actual extent (skip trailing empty rows/cols)
    int32_t last_row = 0, last_col = 0;
    for (int32_t r = 0; r < max_row; ++r) {
        for (int32_t c = 0; c < max_col; ++c) {
            auto v = sheet.get_value({c, r});
            if (!is_empty(v)) {
                last_row = std::max(last_row, r);
                last_col = std::max(last_col, c);
            }
        }
    }

    for (int32_t r = 0; r <= last_row; ++r) {
        for (int32_t c = 0; c <= last_col; ++c) {
            if (c > 0) out << ',';
            auto v = sheet.get_value({c, r});
            std::string display = to_display_string(v);

            // Quote if contains comma, quote, or newline
            if (display.find_first_of(",\"\n") != std::string::npos) {
                out << '"';
                for (char ch : display) {
                    if (ch == '"') out << '"';
                    out << ch;
                }
                out << '"';
            } else {
                out << display;
            }
        }
        out << '\n';
    }
    return out.str();
}

bool CsvIO::import_file(const std::filesystem::path& path, Sheet& sheet) {
    static constexpr uintmax_t MAX_CSV_FILE_SIZE = 500ULL * 1024 * 1024;  // 500 MB
    std::error_code ec;
    auto fsize = std::filesystem::file_size(path, ec);
    if (ec || fsize > MAX_CSV_FILE_SIZE) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream buf;
    buf << file.rdbuf();
    parse(buf.str(), sheet);
    return true;
}

bool CsvIO::export_file(const std::filesystem::path& path, const Sheet& sheet) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << serialize(sheet);
    return file.good();
}

}  // namespace magic
