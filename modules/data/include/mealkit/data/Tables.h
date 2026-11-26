// Vendored copy of module-data v0.1.12; change it in that repo (tools\module.ps1 update), not here.
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
    std::vector<int> lines; // file line of each `row` line, set by readTableText (empty or shorter for text built elsewhere: no line then)
};
// "crate" or "crate (line 12)": how a warning names a row.
std::string rowLabel(const std::string& id, const std::vector<int>& lines, size_t index);
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
// Reads each line with the row type. Unknown fields and bad values go to warnings as "row <id>: unknown field 'speeed'; did you mean 'speed'?".
void tableFromText(const reflect::TypeInfo& rowType, const TableText& text, TableData& out,
                   std::string* warnings = nullptr);
bool loadTableData(const reflect::TypeInfo& rowType, const std::string& path, TableData& out,
                   std::string* warnings = nullptr);
bool saveTableData(const TableData& table, const std::string& path);

// CSV (Google Sheets/Excel): first line is "id" and field paths (same names as the table file), one line per row. Import replaces the table with the CSV contents (one-way).
bool exportTableCsv(const TableData& table, const std::string& path);
bool importTableCsv(TableData& table, const std::string& path, std::string* error);

// What an import would do, without doing it: rows are matched by id. Import replaces the table, so a field with no column in the CSV goes back to its default
// (it shows up as changed cells). `notes` carries what to check before accepting: skipped or unknown columns and ids, and the traps of saving a table from
// a spreadsheet (a value rounded to the cell's decimals, a date made of an id or value, a number reformatted).
struct CsvChange {
    std::string row, column, before, after;
};
struct CsvPreview {
    std::vector<CsvChange> changed;
    std::vector<std::string> added, removed, notes;
    size_t sameRows = 0; // rows in both with no changed cell
};
bool previewTableCsv(const TableData& table, const std::string& path, CsvPreview& out, std::string* error);

// Cells as spreadsheet text. A column is "id" or a field path (the CSV column names). Cells of tab-separated text (TSV) are what Excel and Google Sheets
// copy and paste: cells with a tab, newline or quote are quoted, quotes are doubled, and rows end with a newline.
std::string cellText(const TableData& table, size_t row, const std::string& column);
// Sets one cell from text. false (cell unchanged, reason in error) for a bad id, a duplicate id, a missing path or a value that doesn't parse.
bool setCellText(TableData& table, size_t row, const std::string& column, const std::string& text, std::string* error = nullptr);
// Ranges (ROADMAP item 42, docs/DECISIONS.md 2026-11-23 "Table field ranges"): the `range=min..max` of a top-level field's label
// (MEALKIT_FIELD "name|description|range=0..20", the same attribute the editor uses for slider bounds) is also checked on load and marked in [Tables].
// Warn only: values are never changed, so the content hash and replays stay as the file says. Numbers only (f32, integers, Ticks in seconds).
bool fieldRange(const reflect::FieldInfo& field, double& min, double& max);
// true (and the value in `shown`) if the number is outside min..max. false for other kinds, enums and NaN.
bool valueOutOfRange(const reflect::ValueInfo& value, const void* data, double min, double max, double* shown = nullptr);
// "<id>: field 12 is outside 0..10; ..." for one row (empty = all inside)
std::string rowRangeWarnings(const reflect::TypeInfo& rowType, const void* row, const std::string& id);
// Bulk edit by percentage (ROADMAP item 43; a single value is already set by pasting it over a cell range): multiplies one numeric cell by `factor`
// (1.1 = +10%). Floats and Ticks keep the shortest text that reads back, integers round to the nearest whole number.
enum class ScaleResult : u8 {
    Changed,   // the cell now holds the scaled value
    Unchanged, // the scaled value is the same (0, or a rounded integer that didn't move)
    NotNumber, // id, text, bool, enum, colour, vector ...: left alone
    Failed,    // the new value does not fit the field (reason in error)
};
ScaleResult scaleCell(TableData& table, size_t row, const std::string& column, double factor, std::string* error = nullptr);
std::string formatTsv(const std::vector<std::vector<std::string>>& cells);
std::vector<std::vector<std::string>> parseTsv(const std::string& text);

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
    for (size_t i = 0; i < out.rows.size(); ++i) {
        std::string unknown;
        reflect::fromText(reflect::typeOf<Row>(), &out.rows[i], text.rows[i], &unknown);
        const std::string label = rowLabel(out.ids[i], text.lines, i);
        if (!unknown.empty()) warnings += (warnings.empty() ? "" : "; ") + std::string("row ") + label + ": " + unknown;
        const std::string range = rowRangeWarnings(reflect::typeOf<Row>(), &out.rows[i], label);
        if (!range.empty()) warnings += (warnings.empty() ? "" : "; ") + range;
    }
    if (!warnings.empty()) MEALKIT_WARN("assets/%s: %s", asset, warnings.c_str());
    return true;
}

} // namespace mealkit::data
