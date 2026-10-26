#include "mealkit/data/Tables.h"

#include "mealkit/core/TextFormat.h"
#include "mealkit/platform/Paths.h"

#include <cctype>
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
        for (const TableIndex& t : tables) // 같은 행 타입 (DLL을 바꿔 끼운 뒤에도 맞게 이름으로도)
            if (t.rowType == v.type || std::strcmp(t.rowType->name, v.type->name) == 0) table = &t;
        if (table)
            for (u32 i = 0; i < table->ids->size(); ++i)
                if ((*table->ids)[i] == *ref.id) *ref.index = i;
        if (*ref.index == kNoRow && missing)
            missing->push_back(path + " -> " + *ref.id + " (" + v.type->name + (table ? ")" : ", no table)"));
        break;
    }
    default: break; // Pool은 표·설정 데이터에 없다
    }
}

// CSV 한 칸 (따옴표가 필요하면 감싼다)
std::string csvCell(const std::string& s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

// CSV 전체 → 줄마다 칸 (RFC 4180: 따옴표 안의 쉼표·줄바꿈, "" = ")
std::vector<std::vector<std::string>> parseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> lines;
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false, any = false;
    size_t i = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0; // Excel이 붙이는 BOM
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

} // namespace

bool validRowId(const std::string& id) {
    if (id.empty()) return false;
    for (const char c : id)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
    return true;
}

std::string findTableFile(const char* assetPath) { return findAsset(std::string("assets/") + assetPath); }

bool readTableText(const std::string& path, TableText& out, std::string* warnings) {
    out = {};
    std::string text;
    if (!readTextFile(path, text)) return false;
    size_t start = 0;
    int lineNo = 0;
    bool skipping = false; // 겹친 id의 행
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
        u32 version = 1; // 지금은 버전 1뿐이라 migrate가 없다
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
        if (!unknown.empty() && warnings)
            *warnings += (warnings->empty() ? "" : "; ") + std::string("row ") + text.ids[i] + ": unknown fields " + unknown;
    }
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
    reflect::Instance defaults(*table.rowType); // 열 이름: 기본값 행의 경로 (모든 행이 같은 경로를 낸다)
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
    std::string text;
    if (!readTextFile(path, text)) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    const std::vector<std::vector<std::string>> lines = parseCsv(text);
    if (lines.empty() || lines[0].empty() || lines[0][0] != "id") {
        if (error) *error = "the first row must start with an \"id\" column";
        return false;
    }
    const std::vector<std::string>& header = lines[0];
    TableText out;
    for (size_t l = 1; l < lines.size(); ++l) {
        const std::vector<std::string>& cells = lines[l];
        if (cells.empty() || cells[0].empty()) continue; // 빈 줄
        out.ids.push_back(cells[0]);
        out.rows.emplace_back();
        for (size_t c = 1; c < header.size() && c < cells.size(); ++c)
            if (!header[c].empty()) out.rows.back().emplace_back(header[c], cells[c]);
    }
    std::string warnings;
    TableText checked; // id 검사는 파일 읽기와 같은 규칙
    for (size_t i = 0; i < out.ids.size(); ++i) {
        bool duplicate = false;
        for (const std::string& id : checked.ids) duplicate = duplicate || id == out.ids[i];
        if (!validRowId(out.ids[i]) || duplicate) {
            warnings += (warnings.empty() ? "" : "; ") + std::string(duplicate ? "duplicate" : "invalid") + " id '" + out.ids[i] + "' skipped";
            continue;
        }
        checked.ids.push_back(out.ids[i]);
        checked.rows.push_back(out.rows[i]);
    }
    tableFromText(*table.rowType, checked, table, &warnings);
    if (!warnings.empty()) MEALKIT_WARN("CSV import %s: %s", path.c_str(), warnings.c_str());
    return true;
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
