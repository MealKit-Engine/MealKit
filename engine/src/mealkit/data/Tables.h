#pragma once

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"

#include <string>
#include <vector>

// Definition data table files (assets/tables/*.table, mealkit/sim/Table.h). Game app/, the editor, the content hash, and engine tables (input/sound/feedback) read them with the same functions.
// data/ is a lower layer than the game boundary (game/) (docs/CODING_MODEL.md "Dependencies between engine folders").
//   # MealKit table (PropKind) ...
//   row crate
//   density 200
//   size 0.28 0.64
//   row barrel
//   ...
// Row order = file order = row index. Row ids are letters/digits/_/-/. and unique within a table (on duplicates the later row is dropped).
namespace mealkit::data {

// Table file contents: handled without knowing the format (row id + that row's "path value" lines)
struct TableText {
    std::vector<std::string> ids;
    std::vector<reflect::TextFields> rows;
};
bool readTableText(const std::string& path, TableText& out, std::string* warnings = nullptr);
bool writeTableText(const std::string& path, const TableText& table, const char* rowTypeName);
bool validRowId(const std::string& id);
// Path relative to assets → file (findAssetFile, including module asset paths). Empty string if missing.
std::string findTableFile(const char* assetPath);

// Table handled without a format (editor, content hash): a row type instance per row
struct TableData {
    const reflect::TypeInfo* rowType = nullptr;
    std::vector<std::string> ids;
    std::vector<reflect::Instance> rows;
};
void tableToText(const TableData& table, TableText& out);
// Reads each line with the row type. Unknown fields go to warnings.
void tableFromText(const reflect::TypeInfo& rowType, const TableText& text, TableData& out,
                   std::string* warnings = nullptr);
bool loadTableData(const reflect::TypeInfo& rowType, const std::string& path, TableData& out,
                   std::string* warnings = nullptr);
bool saveTableData(const TableData& table, const std::string& path);

// CSV (Google Sheets/Excel): first line is "id" and field paths (same names as the table file), one line per row. Import replaces the table with the CSV contents (one-way).
bool exportTableCsv(const TableData& table, const std::string& path);
bool importTableCsv(TableData& table, const std::string& path, std::string* error);

// Resolve row references (RowRef): for each RowRef in data, find the id in the referenced table (table with the same row type) and set the row index.
// Missing ids add a line "<where><path> -> <id> (table name)" to missing. Empty id means no reference.
struct TableIndex {
    const reflect::TypeInfo* rowType;
    const std::vector<std::string>* ids;
};
using TableIndexList = std::vector<TableIndex>;
void resolveRows(const reflect::TypeInfo& type, void* data, const TableIndexList& tables,
                 std::vector<std::string>* missing = nullptr, const std::string& where = {});

template <class T>
void resolveRows(T& data, const TableIndexList& tables, std::vector<std::string>* missing = nullptr,
                 const std::string& where = {}) {
    resolveRows(reflect::typeOf<T>(), &data, tables, missing, where);
}

template <class Row>
TableIndex tableIndex(const sim::Table<Row>& table) {
    return {&reflect::typeOf<Row>(), &table.ids};
}

// Reads simulation tables (not kTableView) and hashes ids and row values (excluding MEALKIT_VIEW_FIELD) (content hash).
void hashTables(const reflect::TableList& tables, Hasher& h);

// ---------------------------------------------------------------- Format-aware side (game app/)

// Path registered with MEALKIT_TABLE (found by row type in this module's registration list). nullptr if missing.
template <class Row>
const char* tablePath(const reflect::TableList& list = reflect::moduleTables()) {
    for (const reflect::TableInfo& t : list)
        if (t.rowType == &reflect::typeOf<Row>()) return t.path;
    return nullptr;
}

// Reads the table (registered path, or assetPath if given). If the file is missing, an empty table and a warning.
template <class Row>
bool loadTable(sim::Table<Row>& out, const char* assetPath = nullptr) {
    out.ids.clear();
    out.rows.clear();
    const char* asset = assetPath ? assetPath : tablePath<Row>();
    if (!asset) {
        MEALKIT_ERROR("Table of %s is not registered (MEALKIT_TABLE)", reflect::typeOf<Row>().name);
        return false;
    }
    const std::string path = findTableFile(asset);
    TableText text;
    std::string warnings;
    if (path.empty() || !readTableText(path, text, &warnings)) {
        MEALKIT_WARN("Table assets/%s not found - it has no rows", asset);
        return false;
    }
    out.ids = std::move(text.ids);
    out.rows.resize(out.ids.size());
    for (size_t i = 0; i < out.rows.size(); ++i) reflect::fromText(reflect::typeOf<Row>(), &out.rows[i], text.rows[i], &warnings);
    if (!warnings.empty()) MEALKIT_WARN("assets/%s: %s", asset, warnings.c_str());
    return true;
}

} // namespace mealkit::data
