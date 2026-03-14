#include "io/WorkbookIO.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

// Minimal hand-rolled JSON — no external dependency.
// Format:
// {
//   "sheets": [
//     {
//       "name": "Sheet1",
//       "cols": 26,
//       "rows": 1000,
//       "cells": [ {"c":0,"r":0,"v":42.0}, {"c":1,"r":0,"v":"hello"}, ... ],
//       "formulas": [ {"c":2,"r":0,"f":"A1+B1"}, ... ]
//     }
//   ],
//   "active": 0
// }

namespace magic {

// ── JSON writing helpers ────────────────────────────────────────────────────

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += ch;
        }
    }
    return out;
}

static std::string unescape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"':  out += '"';  ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string WorkbookIO::to_json(const Workbook& workbook) {
    std::ostringstream out;
    out << "{\"sheets\":[";

    for (int si = 0; si < workbook.sheet_count(); ++si) {
        if (si > 0) out << ',';
        const auto& sheet = workbook.sheet(si);
        out << "{\"name\":\"" << escape_json(sheet.name()) << "\""
            << ",\"cols\":" << sheet.col_count()
            << ",\"rows\":" << sheet.row_count();

        // Cells
        out << ",\"cells\":[";
        bool first_cell = true;
        for (int32_t c = 0; c < sheet.col_count(); ++c) {
            for (int32_t r = 0; r < sheet.row_count(); ++r) {
                auto val = sheet.get_value({c, r});
                if (is_empty(val)) continue;

                if (!first_cell) out << ',';
                first_cell = false;

                out << "{\"c\":" << c << ",\"r\":" << r << ",";
                if (is_number(val)) {
                    out << "\"t\":\"n\",\"v\":" << as_number(val);
                } else if (is_string(val)) {
                    out << "\"t\":\"s\",\"v\":\"" << escape_json(as_string(val)) << "\"";
                } else if (is_bool(val)) {
                    out << "\"t\":\"b\",\"v\":" << (std::get<bool>(val) ? "true" : "false");
                } else if (is_error(val)) {
                    out << "\"t\":\"e\",\"v\":" << static_cast<int>(std::get<CellError>(val));
                }
                out << '}';
            }
        }
        out << "]";

        // Formulas
        out << ",\"formulas\":[";
        bool first_formula = true;
        for (int32_t c = 0; c < sheet.col_count(); ++c) {
            for (int32_t r = 0; r < sheet.row_count(); ++r) {
                CellAddress addr{c, r};
                if (!sheet.has_formula(addr)) continue;

                if (!first_formula) out << ',';
                first_formula = false;
                out << "{\"c\":" << c << ",\"r\":" << r
                    << ",\"f\":\"" << escape_json(sheet.get_formula(addr)) << "\"}";
            }
        }
        out << "]}";
    }

    out << "],\"active\":" << workbook.active_index() << ",\"version\":1}";
    return out.str();
}

// ── Minimal JSON reader ─────────────────────────────────────────────────────

namespace {

struct JsonReader {
    const std::string& s;
    size_t pos = 0;

    void skip_ws() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    char peek() { skip_ws(); return pos < s.size() ? s[pos] : '\0'; }
    char next() { skip_ws(); return pos < s.size() ? s[pos++] : '\0'; }

    void expect(char ch) {
        char got = next();
        if (got != ch) throw std::runtime_error(
            std::string("JSON: expected '") + ch + "', got '" + got + "'");
    }

    std::string read_string() {
        expect('"');
        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos;
                switch (s[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += s[pos];
                }
            } else {
                result += s[pos];
            }
            ++pos;
        }
        if (pos < s.size()) ++pos;  // skip closing "
        return result;
    }

    double read_number() {
        skip_ws();
        size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
        while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) ||
               s[pos] == '.')) ++pos;
        // Exponent part: e/E followed by optional sign and digits
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        if (pos == start) throw std::runtime_error("JSON: expected number");
        return std::stod(s.substr(start, pos - start));
    }

    bool read_bool() {
        skip_ws();
        if (s.compare(pos, 4, "true") == 0) { pos += 4; return true; }
        if (s.compare(pos, 5, "false") == 0) { pos += 5; return false; }
        throw std::runtime_error("JSON: expected bool");
    }

    int read_int() { return static_cast<int>(read_number()); }

    // Skip key: "key":
    void read_key(const std::string& expected) {
        std::string key = read_string();
        if (key != expected) throw std::runtime_error("JSON: expected key '" + expected + "', got '" + key + "'");
        expect(':');
    }

    // Skip to a key within the current object
    bool find_key(const std::string& key) {
        // Simple: scan for "key":
        while (peek() != '}' && peek() != '\0') {
            std::string k = read_string();
            expect(':');
            if (k == key) return true;
            skip_value();
            if (peek() == ',') next();
        }
        return false;
    }

    void skip_value() {
        skip_ws();
        if (s[pos] == '"') { read_string(); return; }
        if (s[pos] == '{') { skip_object(); return; }
        if (s[pos] == '[') { skip_array(); return; }
        // number, bool, null
        while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' &&
               !std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    void skip_object() {
        expect('{');
        if (peek() == '}') { next(); return; }
        while (true) {
            read_string(); expect(':'); skip_value();
            if (peek() == ',') { next(); continue; }
            break;
        }
        expect('}');
    }

    void skip_array() {
        expect('[');
        if (peek() == ']') { next(); return; }
        while (true) {
            skip_value();
            if (peek() == ',') { next(); continue; }
            break;
        }
        expect(']');
    }
};

}  // namespace

bool WorkbookIO::from_json(const std::string& json, Workbook& workbook) {
    try {
        JsonReader r{json};
        r.expect('{');

        // Clear existing workbook — build fresh
        // We'll construct sheets and swap at the end
        std::vector<Sheet> sheets;
        int active = 0;

        while (r.peek() != '}') {
            std::string key = r.read_string();
            r.expect(':');

            if (key == "sheets") {
                r.expect('[');
                while (r.peek() != ']') {
                    r.expect('{');
                    std::string name = "Sheet";
                    int cols = 26, rows = 1000;
                    std::vector<std::tuple<int, int, CellValue>> cells;
                    std::vector<std::tuple<int, int, std::string>> formulas;

                    while (r.peek() != '}') {
                        std::string k = r.read_string();
                        r.expect(':');

                        if (k == "name") {
                            name = r.read_string();
                        } else if (k == "cols") {
                            cols = std::min(r.read_int(), 16384);
                        } else if (k == "rows") {
                            rows = std::min(r.read_int(), 1048576);
                        } else if (k == "cells") {
                            r.expect('[');
                            while (r.peek() != ']') {
                                r.expect('{');
                                int c = 0, r2 = 0;
                                std::string type;
                                // Two-pass: store raw value string, then parse after type is known
                                std::string raw_v;
                                bool has_v = false;

                                while (r.peek() != '}') {
                                    std::string ck = r.read_string();
                                    r.expect(':');
                                    if (ck == "c") c = r.read_int();
                                    else if (ck == "r") r2 = r.read_int();
                                    else if (ck == "t") type = r.read_string();
                                    else if (ck == "v") {
                                        // Capture raw value text for deferred parsing
                                        size_t v_start = r.pos;
                                        r.skip_ws();
                                        v_start = r.pos;
                                        r.skip_value();
                                        raw_v = r.s.substr(v_start, r.pos - v_start);
                                        has_v = true;
                                    } else {
                                        r.skip_value();
                                    }
                                    if (r.peek() == ',') r.next();
                                }
                                r.expect('}');

                                // Now interpret the value based on type
                                CellValue val;
                                if (has_v && !type.empty()) {
                                    if (type == "n") val = CellValue{std::stod(raw_v)};
                                    else if (type == "s") {
                                        // Strip surrounding quotes and unescape
                                        if (raw_v.size() >= 2 && raw_v.front() == '"' && raw_v.back() == '"')
                                            val = CellValue{unescape_json(raw_v.substr(1, raw_v.size() - 2))};
                                        else
                                            val = CellValue{unescape_json(raw_v)};
                                    }
                                    else if (type == "b") val = CellValue{raw_v == "true"};
                                    else if (type == "e") val = CellValue{static_cast<CellError>(std::stoi(raw_v))};
                                }
                                // Skip cells with out-of-range coordinates
                                static constexpr int MAX_COL = 16384;
                                static constexpr int MAX_ROW = 1048576;
                                if (c >= 0 && c < MAX_COL && r2 >= 0 && r2 < MAX_ROW)
                                    cells.emplace_back(c, r2, val);
                                if (r.peek() == ',') r.next();
                            }
                            r.expect(']');
                        } else if (k == "formulas") {
                            r.expect('[');
                            while (r.peek() != ']') {
                                r.expect('{');
                                int c = 0, r2 = 0;
                                std::string f;

                                while (r.peek() != '}') {
                                    std::string fk = r.read_string();
                                    r.expect(':');
                                    if (fk == "c") c = r.read_int();
                                    else if (fk == "r") r2 = r.read_int();
                                    else if (fk == "f") f = r.read_string();
                                    else r.skip_value();
                                    if (r.peek() == ',') r.next();
                                }
                                r.expect('}');
                                // Skip formulas with out-of-range coordinates
                                if (c >= 0 && c < 16384 && r2 >= 0 && r2 < 1048576)
                                    formulas.emplace_back(c, r2, f);
                                if (r.peek() == ',') r.next();
                            }
                            r.expect(']');
                        } else {
                            r.skip_value();
                        }
                        if (r.peek() == ',') r.next();
                    }
                    r.expect('}');

                    Sheet sheet(name, cols, rows);
                    for (const auto& [c, row, val] : cells)
                        sheet.set_value({c, row}, val);
                    for (const auto& [c, row, formula] : formulas)
                        sheet.set_formula({c, row}, formula);
                    sheets.push_back(std::move(sheet));

                    if (r.peek() == ',') r.next();
                }
                r.expect(']');
            } else if (key == "active") {
                active = r.read_int();
            } else {
                r.skip_value();
            }
            if (r.peek() == ',') r.next();
        }

        // Rebuild workbook
        if (sheets.empty()) return false;

        // Remove existing sheets and add loaded ones
        while (workbook.sheet_count() > 1)
            workbook.remove_sheet(workbook.sheet_count() - 1);

        // Replace first sheet
        workbook.sheet(0) = std::move(sheets[0]);

        // Add remaining sheets
        for (size_t i = 1; i < sheets.size(); ++i) {
            workbook.add_sheet(sheets[i].name());
            workbook.sheet(static_cast<int>(i)) = std::move(sheets[i]);
        }

        if (active >= 0 && active < workbook.sheet_count())
            workbook.set_active(active);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool WorkbookIO::save(const std::filesystem::path& path, const Workbook& workbook) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << to_json(workbook);
    return file.good();
}

bool WorkbookIO::load(const std::filesystem::path& path, Workbook& workbook) {
    static constexpr uintmax_t MAX_WORKBOOK_FILE_SIZE = 100ULL * 1024 * 1024;  // 100 MB
    std::error_code ec;
    auto fsize = std::filesystem::file_size(path, ec);
    if (ec || fsize > MAX_WORKBOOK_FILE_SIZE) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::ostringstream buf;
    buf << file.rdbuf();
    return from_json(buf.str(), workbook);
}

}  // namespace magic
