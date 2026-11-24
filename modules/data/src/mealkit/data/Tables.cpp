// Vendored copy of module-data v0.1.10; change it in that repo (tools\module.ps1 update), not here.
#include "mealkit/data/Tables.h"

#include "mealkit/core/TextFormat.h"
#include "mealkit/platform/Paths.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mealkit::data {
namespace {

void trimRight(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

const u8* at(const void* data, u32 offset) { return static_cast<const u8*>(data) + offset; }
u8* at(void* data, u32 offset) { return static_cast<u8*>(data) + offset; }

void resolveValue(const reflect::ValueInfo& v, void* data, const TableIndexList& tables,
                  std::vector<std::string>* missing, const std::string& path);

void resolveStruct(const reflect::TypeInfo& type, void* data, const TableIndexList& tables,
                   std::vector<std::string>* missing, const std::string& prefix) {
    for (const reflect::FieldInfo& f : type.fields) resolveValue(*f.value, at(data, f.offset), tables, missing, prefix + f.name);
}

void resolveValue(const reflect::ValueInfo& v, void* data, const TableIndexList& tables,
                  std::vector<std::string>* missing, const std::string& path) {
    switch (v.kind) {
    case reflect::Kind::Struct: resolveStruct(*v.type, data, tables, missing, path + "."); break;
    case reflect::Kind::Array:
        for (u32 i = 0; i < v.count; ++i)
            resolveValue(*v.element, at(data, i * v.element->size), tables, missing, path + "[" + std::to_string(i) + "]");
        break;
    case reflect::Kind::RowRef: {
        const reflect::RowRefView ref = reflect::rowRefOf(data);
        *ref.index = kNoRow;
        if (ref.id->empty()) break;
        const TableIndex* table = nullptr;
        for (const TableIndex& t : tables) // same row type (also by name, so it still matches after swapping the DLL)
            if (t.rowType == v.type || std::strcmp(t.rowType->name, v.type->name) == 0) table = &t;
        if (table)
            for (u32 i = 0; i < table->ids->size(); ++i)
                if ((*table->ids)[i] == *ref.id) *ref.index = i;
        if (*ref.index == kNoRow && missing)
            missing->push_back(path + " -> " + *ref.id + " (" + v.type->name + (table ? ")" : ", no table)"));
        break;
    }
    default: break; // Pools don't appear in table/settings data
    }
}

// One CSV cell (quoted if needed)
std::string csvCell(const std::string& s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

// Whole CSV → cells per line (RFC 4180: commas/newlines inside quotes, "" = ")
std::vector<std::vector<std::string>> parseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false, any = false;
    size_t i = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0; // BOM added by Excel
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
                cell += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                cell += c;
            }
            continue;
        }
        if (c == '"') {
            quoted = any = true;
        } else if (c == ',') {
            cells.push_back(cell);
            cell.clear();
            any = true;
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            if (any || !cell.empty()) {
                cells.push_back(cell);
                lines.push_back(cells);
            }
            cells.clear();
            cell.clear();
            any = false;
        } else {
            cell += c;
            any = true;
        }
    }
    if (any || !cell.empty()) {
        cells.push_back(cell);
        lines.push_back(cells);
    }
    return lines;
}

bool validUtf8(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const size_t extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : 99;
        if (extra == 99 || i + extra >= s.size()) return false;
        for (size_t k = 1; k <= extra; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        i += extra + 1;
    }
    return true;
}

bool numberOf(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && !*end;
}

// "1-Feb", "Feb-10", "2024-03-05", "3/4": what a spreadsheet makes of "1-2" or "3/4" when it converts a cell on its own
bool looksLikeDate(const std::string& s) {
    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    size_t digits = 0;
    while (digits < s.size() && std::isdigit(static_cast<unsigned char>(s[digits]))) ++digits;
    if (digits && digits <= 2 && s.size() > digits + 1 && s[digits] == '-')
        for (const char* m : months)
            if (s.compare(digits + 1, std::string::npos, m) == 0) return true;
    for (const char* m : months)
        if (s.compare(0, 3, m) == 0 && s.size() > 4 && s[3] == '-' && s.find_first_not_of("0123456789", 4) == std::string::npos) return true;
    int parts = 0;
    size_t i = 0;
    char sep = 0;
    while (i < s.size()) {
        size_t n = 0;
        while (i + n < s.size() && std::isdigit(static_cast<unsigned char>(s[i + n]))) ++n;
        if (!n || n > 4) return false;
        ++parts;
        i += n;
        if (i == s.size()) break;
        if ((s[i] != '/' && s[i] != '-') || (sep && s[i] != sep)) return false;
        sep = s[i];
        ++i;
        if (i == s.size()) return false;
    }
    return (sep == '/' && (parts == 2 || parts == 3)) || (sep == '-' && parts == 3 && s.size() == 10 && s[4] == '-');
}

// Why a changed value looks like the spreadsheet's doing rather than an edit. Empty when it doesn't.
std::string spreadsheetTrap(const std::string& before, const std::string& after) {
    if (before == after) return {};
    if (looksLikeDate(after) && !looksLikeDate(before)) return "looks like a date made by the spreadsheet";
    double b = 0, a = 0;
    if (!numberOf(before, b) || !numberOf(after, a)) return {};
    if (b == a) return "number written another way (leading zeros or trailing zeros dropped?)";
    const size_t dot = after.find('.');
    const size_t decimals = dot == std::string::npos ? 0 : after.size() - dot - 1;
    const size_t beforeDot = before.find('.');
    const size_t beforeDecimals = beforeDot == std::string::npos ? 0 : before.size() - beforeDot - 1;
    if (beforeDecimals > decimals) {
        char rounded[64];
        std::snprintf(rounded, sizeof(rounded), "%.*f", static_cast<int>(decimals), b);
        if (after == rounded) return "rounded to " + std::to_string(decimals) + " decimals (a spreadsheet saves what the cell shows)";
    }
    return {};
}

// Reads a CSV file into table text (rows by the header's column names). false with the reason for a file that can't be a table.
bool readCsvText(const std::string& path, TableText& out, std::vector<std::string>& header, std::string& warnings, std::string* error) {
    std::string text;
    if (!readTextFile(path, text)) {
        if (error) *error = "cannot open " + path + " (is it open in a spreadsheet?)";
        return false;
    }
    if (!validUtf8(text)) {
        if (error) *error = "the file is not UTF-8 (save it as \"CSV UTF-8\", not plain \"CSV\")";
        return false;
    }
    const std::vector<std::vector<std::string>> lines = parseCsv(text);
    if (lines.empty() || lines[0].empty() || lines[0][0] != "id") {
        if (error) *error = "the first row must start with an \"id\" column";
        return false;
    }
    header = lines[0];
    TableText raw;
    for (size_t l = 1; l < lines.size(); ++l) {
        const std::vector<std::string>& cells = lines[l];
        if (cells.empty() || cells[0].empty()) continue; // empty line
        raw.ids.push_back(cells[0]);
        raw.rows.emplace_back();
        for (size_t c = 1; c < header.size() && c < cells.size(); ++c)
            if (!header[c].empty()) raw.rows.back().emplace_back(header[c], cells[c]);
    }
    out = {};
    for (size_t i = 0; i < raw.ids.size(); ++i) { // id checks use the same rules as file reading
        bool duplicate = false;
        for (const std::string& id : out.ids) duplicate = duplicate || id == raw.ids[i];
        if (!validRowId(raw.ids[i]) || duplicate) {
            warnings += (warnings.empty() ? "" : "; ") + std::string(duplicate ? "duplicate" : "invalid") + " id '" + raw.ids[i] + "' skipped";
            continue;
        }
        out.ids.push_back(raw.ids[i]);
        out.rows.push_back(raw.rows[i]);
    }
    return true;
}

} // namespace

bool validRowId(const std::string& id) {
    if (id.empty()) return false;
    for (const char c : id)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
    return true;
}

std::string findTableFile(const char* assetPath) { return findAssetFile(assetPath); }

std::string rowLabel(const std::string& id, const std::vector<int>& lines, size_t index) {
    if (index >= lines.size() || lines[index] <= 0) return id;
    return id + " (line " + std::to_string(lines[index]) + ")";
}

bool readTableText(const std::string& path, TableText& out, std::string* warnings) {
    out = {};
    std::string text;
    if (!readTextFile(path, text)) return false;
    size_t start = 0;
    int lineNo = 0;
    bool skipping = false; // rows with a duplicate id
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        ++lineNo;
        trimRight(line);
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        line = line.substr(first);
        u32 version = 1; // only version 1 for now, so no migrate
        if (text::readFormatLine(line, text::kTable, path, version)) continue;
        const size_t space = line.find(' ');
        const std::string key = line.substr(0, space);
        const std::string value = space == std::string::npos ? std::string() : line.substr(line.find_first_not_of(' ', space));
        if (key == "row") {
            skipping = false;
            bool duplicate = false;
            for (const std::string& id : out.ids) duplicate = duplicate || id == value;
            if (!validRowId(value) || duplicate) {
                if (warnings)
                    *warnings += (warnings->empty() ? "" : "; ") + std::string("line ") + std::to_string(lineNo) +
                                 (duplicate ? ": duplicate row id '" : ": invalid row id '") + value + "' skipped";
                skipping = true;
                continue;
            }
            out.ids.push_back(value);
            out.rows.emplace_back();
            out.lines.push_back(lineNo);
            continue;
        }
        if (skipping) continue;
        if (out.rows.empty()) {
            if (warnings) *warnings += (warnings->empty() ? "" : "; ") + std::string("line ") + std::to_string(lineNo) + ": value before the first row";
            continue;
        }
        out.rows.back().emplace_back(key, value);
    }
    return true;
}

bool writeTableText(const std::string& path, const TableText& table, const char* rowTypeName) {
    std::string text = std::string("# MealKit table (") + rowTypeName +
                       "). Edit it in the editor's [Tables] panel or import a CSV there.\n" + text::formatLine(text::kTable) + "\n";
    for (size_t i = 0; i < table.ids.size(); ++i) {
        text += "\nrow " + table.ids[i] + "\n";
        for (const auto& [key, value] : table.rows[i]) text += key + " " + value + "\n";
    }
    if (!writeTextFile(path, text)) {
        MEALKIT_ERROR("Cannot write %s", path.c_str());
        return false;
    }
    return true;
}

void tableToText(const TableData& table, TableText& out) {
    out = {};
    out.ids = table.ids;
    for (const reflect::Instance& row : table.rows) {
        out.rows.emplace_back();
        reflect::toText(*table.rowType, row.data(), out.rows.back());
    }
}

void tableFromText(const reflect::TypeInfo& rowType, const TableText& text, TableData& out, std::string* warnings) {
    out = {};
    out.rowType = &rowType;
    out.ids = text.ids;
    for (size_t i = 0; i < text.rows.size(); ++i) {
        out.rows.emplace_back(rowType);
        std::string unknown;
        reflect::fromText(rowType, out.rows.back().data(), text.rows[i], &unknown);
        const std::string label = rowLabel(text.ids[i], text.lines, i);
        if (!unknown.empty() && warnings)
            *warnings += (warnings->empty() ? "" : "; ") + std::string("row ") + label + ": " + unknown;
        if (warnings) {
            const std::string range = rowRangeWarnings(rowType, out.rows.back().data(), label);
            if (!range.empty()) *warnings += (warnings->empty() ? "" : "; ") + range;
        }
    }
}

ScaleResult scaleCell(TableData& table, size_t row, const std::string& column, double factor, std::string* error) {
    const reflect::ValueInfo* value = nullptr;
    void* data = nullptr;
    if (row >= table.rows.size() || column == "id" || !table.rowType ||
        !reflect::resolvePath(*table.rowType, table.rows[row].data(), column, value, data) || !value)
        return ScaleResult::NotNumber;
    std::string text;
    char buffer[32];
    switch (value->kind) {
    case reflect::Kind::Float: {
        const f32 before = *static_cast<const f32*>(data);
        const f32 after = static_cast<f32>(static_cast<double>(before) * factor);
        if (after == before) return ScaleResult::Unchanged;
        text::writeFloat(buffer, sizeof(buffer), after);
        text = buffer;
        break;
    }
    case reflect::Kind::Ticks: {
        const f32 before = static_cast<const Ticks*>(data)->toSeconds();
        const f32 after = static_cast<f32>(static_cast<double>(before) * factor);
        if (after == before) return ScaleResult::Unchanged;
        text::writeFloat(buffer, sizeof(buffer), after);
        text = buffer;
        break;
    }
    case reflect::Kind::Int:
    case reflect::Kind::UInt: {
        if (!value->enumNames.empty()) return ScaleResult::NotNumber;
        const bool isSigned = value->kind == reflect::Kind::Int;
        double before = 0.0;
        switch (value->size) {
        case 1: before = isSigned ? static_cast<double>(*static_cast<const i8*>(data)) : static_cast<double>(*static_cast<const u8*>(data)); break;
        case 2: before = isSigned ? static_cast<double>(*static_cast<const i16*>(data)) : static_cast<double>(*static_cast<const u16*>(data)); break;
        case 4: before = isSigned ? static_cast<double>(*static_cast<const i32*>(data)) : static_cast<double>(*static_cast<const u32*>(data)); break;
        case 8: before = isSigned ? static_cast<double>(*static_cast<const i64*>(data)) : static_cast<double>(*static_cast<const u64*>(data)); break;
        default: return ScaleResult::NotNumber;
        }
        const double after = std::round(before * factor);
        if (after == before) return ScaleResult::Unchanged;
        std::snprintf(buffer, sizeof(buffer), "%.0f", after);
        text = buffer;
        break;
    }
    default: return ScaleResult::NotNumber;
    }
    return setCellText(table, row, column, text, error) ? ScaleResult::Changed : ScaleResult::Failed;
}

bool fieldRange(const reflect::FieldInfo& field, double& min, double& max) {
    const char* at = field.label ? std::strstr(field.label, "|range=") : nullptr;
    if (!at) return false;
    at += 7;
    // "0..10": strtod would take "0." of "0..10" as the number, so split at ".." first
    const char* dots = std::strstr(at, "..");
    if (!dots || dots == at) return false;
    const std::string first(at, dots);
    char* end = nullptr;
    const double lo = std::strtod(first.c_str(), &end);
    if (!end || *end) return false;
    char* end2 = nullptr;
    const double hi = std::strtod(dots + 2, &end2);
    if (end2 == dots + 2 || !(hi > lo)) return false;
    min = lo;
    max = hi;
    return true;
}

bool valueOutOfRange(const reflect::ValueInfo& value, const void* data, double min, double max, double* shown) {
    double v = 0.0;
    switch (value.kind) {
    case reflect::Kind::Float: v = *static_cast<const f32*>(data); break;
    case reflect::Kind::Ticks: v = static_cast<const Ticks*>(data)->toSeconds(); break;
    case reflect::Kind::Int:
    case reflect::Kind::UInt: {
        if (!value.enumNames.empty()) return false; // enums are names in the file
        const bool isSigned = value.kind == reflect::Kind::Int;
        switch (value.size) {
        case 1: v = isSigned ? static_cast<double>(*static_cast<const i8*>(data)) : static_cast<double>(*static_cast<const u8*>(data)); break;
        case 2: v = isSigned ? static_cast<double>(*static_cast<const i16*>(data)) : static_cast<double>(*static_cast<const u16*>(data)); break;
        case 4: v = isSigned ? static_cast<double>(*static_cast<const i32*>(data)) : static_cast<double>(*static_cast<const u32*>(data)); break;
        case 8: v = isSigned ? static_cast<double>(*static_cast<const i64*>(data)) : static_cast<double>(*static_cast<const u64*>(data)); break;
        default: return false;
        }
        break;
    }
    default: return false;
    }
    if (shown) *shown = v;
    return v < min || v > max; // NaN compares false: not marked
}

std::string rowRangeWarnings(const reflect::TypeInfo& rowType, const void* row, const std::string& id) {
    std::string out;
    for (const reflect::FieldInfo& f : rowType.fields) {
        double lo = 0.0, hi = 0.0, v = 0.0;
        if (!fieldRange(f, lo, hi)) continue;
        if (!valueOutOfRange(*f.value, static_cast<const char*>(row) + f.offset, lo, hi, &v)) continue;
        char text[160];
        std::snprintf(text, sizeof(text), "%s: %s %.6g is outside %.6g..%.6g", id.c_str(), f.name, v, lo, hi);
        out += (out.empty() ? "" : "; ") + std::string(text);
    }
    return out;
}

bool loadTableData(const reflect::TypeInfo& rowType, const std::string& path, TableData& out, std::string* warnings) {
    TableText text;
    if (!readTableText(path, text, warnings)) {
        out = {};
        out.rowType = &rowType;
        return false;
    }
    tableFromText(rowType, text, out, warnings);
    return true;
}

bool saveTableData(const TableData& table, const std::string& path) {
    TableText text;
    tableToText(table, text);
    return writeTableText(path, text, table.rowType->name);
}

bool exportTableCsv(const TableData& table, const std::string& path) {
    reflect::Instance defaults(*table.rowType); // column names: paths of the default row (every row yields the same paths)
    reflect::TextFields columns;
    reflect::toText(*table.rowType, defaults.data(), columns);
    std::string text = "id";
    for (const auto& [key, value] : columns) text += "," + csvCell(key);
    text += "\n";
    for (size_t i = 0; i < table.rows.size(); ++i) {
        reflect::TextFields fields;
        reflect::toText(*table.rowType, table.rows[i].data(), fields);
        text += csvCell(table.ids[i]);
        for (const auto& [key, value] : fields) text += "," + csvCell(value);
        text += "\n";
    }
    if (!writeTextFile(path, text)) {
        MEALKIT_ERROR("Cannot write %s", path.c_str());
        return false;
    }
    return true;
}

bool importTableCsv(TableData& table, const std::string& path, std::string* error) {
    TableText checked;
    std::vector<std::string> header;
    std::string warnings;
    if (!readCsvText(path, checked, header, warnings, error)) return false;
    tableFromText(*table.rowType, checked, table, &warnings);
    if (!warnings.empty()) MEALKIT_WARN("CSV import %s: %s", path.c_str(), warnings.c_str());
    return true;
}

bool previewTableCsv(const TableData& table, const std::string& path, CsvPreview& out, std::string* error) {
    out = {};
    TableText checked;
    std::vector<std::string> header;
    std::string warnings;
    if (!readCsvText(path, checked, header, warnings, error)) return false;
    if (!warnings.empty()) out.notes.push_back(warnings);
    TableData next;
    std::string unknown;
    tableFromText(*table.rowType, checked, next, &unknown);
    if (!unknown.empty()) out.notes.push_back(unknown);

    reflect::Instance defaults(*table.rowType);
    reflect::TextFields columns;
    reflect::toText(*table.rowType, defaults.data(), columns);
    for (size_t c = 1; c < header.size(); ++c) {
        bool known = false;
        for (const auto& [key, value] : columns) known = known || key == header[c];
        if (!known) out.notes.push_back("column '" + header[c] + "' is not a field of this table and is ignored");
    }
    for (const auto& [key, value] : columns) {
        bool present = false;
        for (const std::string& h : header) present = present || h == key;
        if (!present) out.notes.push_back("column '" + key + "' is not in the file: its values go back to the default");
    }

    constexpr size_t kMaxTrapNotes = 20;
    size_t traps = 0;
    for (size_t r = 0; r < next.ids.size(); ++r) {
        size_t old = table.ids.size();
        for (size_t i = 0; i < table.ids.size(); ++i)
            if (table.ids[i] == next.ids[r]) old = i;
        if (old == table.ids.size()) {
            out.added.push_back(next.ids[r]);
            if (looksLikeDate(next.ids[r]) && traps++ < kMaxTrapNotes) out.notes.push_back("new id '" + next.ids[r] + "' looks like a date made by the spreadsheet");
            continue;
        }
        reflect::TextFields before, after;
        reflect::toText(*table.rowType, table.rows[old].data(), before);
        reflect::toText(*table.rowType, next.rows[r].data(), after);
        bool same = true;
        for (size_t k = 0; k < before.size() && k < after.size(); ++k) {
            if (before[k].second == after[k].second) continue;
            same = false;
            out.changed.push_back({next.ids[r], before[k].first, before[k].second, after[k].second});
            const std::string trap = spreadsheetTrap(before[k].second, after[k].second);
            if (!trap.empty() && traps++ < kMaxTrapNotes)
                out.notes.push_back(next.ids[r] + " " + before[k].first + ": " + before[k].second + " -> " + after[k].second + " " + trap);
        }
        if (same) ++out.sameRows;
    }
    if (traps > kMaxTrapNotes) out.notes.push_back(std::to_string(traps - kMaxTrapNotes) + " more values look changed by the spreadsheet");
    for (const std::string& id : table.ids) {
        bool kept = false;
        for (const std::string& n : next.ids) kept = kept || n == id;
        if (!kept) out.removed.push_back(id);
    }
    return true;
}

std::string cellText(const TableData& table, size_t row, const std::string& column) {
    if (row >= table.rows.size()) return {};
    if (column == "id") return table.ids[row];
    reflect::TextFields fields;
    reflect::toText(*table.rowType, table.rows[row].data(), fields);
    for (const auto& [key, value] : fields)
        if (key == column) return value;
    return {};
}

bool setCellText(TableData& table, size_t row, const std::string& column, const std::string& text, std::string* error) {
    if (row >= table.rows.size()) {
        if (error) *error = "no such row";
        return false;
    }
    if (column == "id") {
        if (text == table.ids[row]) return true;
        bool duplicate = false;
        for (size_t i = 0; i < table.ids.size(); ++i) duplicate = duplicate || (i != row && table.ids[i] == text);
        if (!validRowId(text) || duplicate) {
            if (error) *error = std::string(duplicate ? "duplicate id '" : "invalid id '") + text + "'";
            return false;
        }
        table.ids[row] = text;
        return true;
    }
    if (!reflect::setFromText(*table.rowType, table.rows[row].data(), column, text)) {
        if (error) *error = "'" + text + "' is not a value for " + column;
        return false;
    }
    return true;
}

std::string formatTsv(const std::vector<std::vector<std::string>>& cells) {
    std::string out;
    for (const std::vector<std::string>& line : cells) {
        for (size_t c = 0; c < line.size(); ++c) {
            if (c) out += '\t';
            if (line[c].find_first_of("\t\"\r\n") == std::string::npos) {
                out += line[c];
                continue;
            }
            out += '"';
            for (const char ch : line[c]) {
                if (ch == '"') out += '"';
                out += ch;
            }
            out += '"';
        }
        out += '\n';
    }
    return out;
}

std::vector<std::vector<std::string>> parseTsv(const std::string& text) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false, any = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
                cell += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                cell += c;
            }
            continue;
        }
        if (c == '"' && cell.empty()) { // a quote only opens a cell at its start (a quote inside plain text is text)
            quoted = any = true;
        } else if (c == '\t') {
            cells.push_back(cell);
            cell.clear();
            any = true;
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            cells.push_back(cell);
            lines.push_back(cells);
            cells.clear();
            cell.clear();
            any = false;
        } else {
            cell += c;
            any = true;
        }
    }
    if (any || !cell.empty()) { // no newline after the last line
        cells.push_back(cell);
        lines.push_back(cells);
    }
    return lines;
}

void resolveRows(const reflect::TypeInfo& type, void* data, const TableIndexList& tables,
                 std::vector<std::string>* missing, const std::string& where) {
    resolveStruct(type, data, tables, missing, where);
}

void hashTables(const reflect::TableList& tables, Hasher& h) {
    for (const reflect::TableInfo& info : tables) {
        if (info.flags & reflect::kTableView) continue;
        const std::string path = findTableFile(info.path);
        TableData table;
        h.bytes(info.path, std::strlen(info.path));
        if (path.empty() || !loadTableData(*info.rowType, path, table)) {
            h.add(0ull);
            continue;
        }
        h.add(static_cast<u64>(table.ids.size()));
        for (size_t i = 0; i < table.ids.size(); ++i) {
            h.add(static_cast<u64>(table.ids[i].size()));
            h.bytes(table.ids[i].data(), table.ids[i].size());
            reflect::hashStruct(*info.rowType, table.rows[i].data(), h, reflect::kFieldView);
        }
    }
}

} // namespace mealkit::data
