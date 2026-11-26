// Vendored copy of module-data v0.1.12; change it in that repo (tools\module.ps1 update), not here.
#include "mealkit/editor/TablesPanel.h"

#include "mealkit/core/Log.h"
#include "mealkit/editor/FieldEditor.h"
#include "mealkit/editor/Strings.h"
#include "mealkit/platform/FileDialog.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <imgui.h>

namespace mealkit::editor {
namespace {

const FileFilter kCsvFilters[] = {{"CSV (*.csv)", "*.csv"}, {"All files (*.*)", "*.*"}};

std::string backslashes(std::string path) {
    path = nativePath(path);
    return path;
}

} // namespace

std::string TablesPanel::csvPathOf(const Doc& doc) const {
    for (const auto& [table, file] : m_csvPaths)
        if (table == doc.info.path) return isAbsolutePath(file) ? file : nativePath(m_projectDir + "/" + file);
    return {};
}

void TablesPanel::rememberCsvPath(const Doc& doc, const std::string& path) {
    std::string stored = path;
    const std::string prefix = nativePath(m_projectDir + "/");
    if (stored.compare(0, prefix.size(), prefix) == 0) stored = stored.substr(prefix.size());
    for (auto& entry : m_csvPaths)
        if (entry.first == doc.info.path) {
            entry.second = stored;
            stored.clear();
            break;
        }
    if (!stored.empty()) m_csvPaths.emplace_back(doc.info.path, stored);
    std::string text;
    for (const auto& [table, file] : m_csvPaths) text += table + " " + file + "\n";
    createDirectories(m_projectDir + "/build/editor");
    writeTextFile(m_projectDir + "/build/editor/table_csv.txt", text);
}

void TablesPanel::loadCsvPaths() {
    m_csvPaths.clear();
    std::string text;
    if (m_projectDir.empty() || !readTextFile(m_projectDir + "/build/editor/table_csv.txt", text)) return;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        while (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t space = line.find(' ');
        if (space != std::string::npos && space > 0 && space + 1 < line.size()) m_csvPaths.emplace_back(line.substr(0, space), line.substr(space + 1));
    }
}

void TablesPanel::setProjectDirectory(const std::string& directory) {
    m_projectDir = directory;
    loadCsvPaths();
    for (Doc& doc : m_docs) {
        doc.path = backslashes(m_projectDir + "/" + assetFilePath(doc.info.path));
        if (doc.data.rowType) load(doc);
    }
}

void TablesPanel::setTables(const reflect::TableList* tables) {
    if (!tables) { // unloading the DLL: game table values are held only as text
        for (Doc& doc : m_docs) {
            if (!doc.data.rowType) continue;
            bool engineTable = false;
            for (const reflect::TableInfo& e : m_engineTables) engineTable = engineTable || e.rowType == doc.info.rowType;
            if (engineTable) continue;
            data::tableToText(doc.data, doc.backup);
            doc.data = {};
            doc.info.rowType = nullptr;
        }
        rebuildIndex();
        return;
    }
    std::vector<reflect::TableInfo> infos = m_engineTables;
    infos.insert(infos.end(), tables->begin(), tables->end());
    syncDocs(infos);
}

void TablesPanel::setEngineTables(const reflect::TableList& tables) {
    m_engineTables = tables;
    if (m_docs.empty()) syncDocs(m_engineTables); // engine tables are visible even before game code is loaded
}

void TablesPanel::syncDocs(const std::vector<reflect::TableInfo>& infos) {
    std::vector<Doc> next;
    for (const reflect::TableInfo& info : infos) {
        auto it = std::find_if(m_docs.begin(), m_docs.end(), [&](const Doc& d) { return d.path.empty() ? false : std::strcmp(d.info.path, info.path) == 0; });
        Doc doc;
        if (it != m_docs.end()) {
            doc = std::move(*it);
            const bool reloadType = !doc.data.rowType;
            doc.info = info;
            if (reloadType) { // rebuild from text with the new DLL's type
                std::string warnings;
                data::tableFromText(*info.rowType, doc.backup, doc.data, &warnings);
                if (!warnings.empty()) MEALKIT_WARN("Table %s: %s", info.path, warnings.c_str());
                doc.undo.reset(doc.backup);
            }
        } else {
            doc.info = info;
            doc.path = backslashes(m_projectDir + "/" + assetFilePath(info.path));
            load(doc);
        }
        reflect::Instance defaults(*info.rowType);
        reflect::TextFields fields;
        reflect::toText(*info.rowType, defaults.data(), fields);
        doc.columns.clear();
        for (const auto& [key, value] : fields) doc.columns.push_back(key);
        next.push_back(std::move(doc));
    }
    m_docs = std::move(next);
    if (m_current >= static_cast<int>(m_docs.size())) m_current = 0;
    rebuildIndex();
}

void TablesPanel::load(Doc& doc) {
    std::string warnings;
    if (!fileExists(doc.path)) {
        doc.data = {};
        doc.data.rowType = doc.info.rowType;
        doc.status = tr("No file yet - it is created when you save");
    } else if (!data::loadTableData(*doc.info.rowType, doc.path, doc.data, &warnings)) {
        doc.status = trf("Could not read %s", doc.path.c_str());
    } else {
        doc.status = warnings;
    }
    doc.dirty = false;
    doc.selected = -1;
    rebuildIndex();
    data::TableText now;
    if (doc.data.rowType) data::tableToText(doc.data, now);
    doc.undo.reset(std::move(now));
}

bool TablesPanel::save(Doc& doc) {
    if (!doc.data.rowType) return false;
    createDirectories(parentDirectory(doc.path));
    if (!data::saveTableData(doc.data, doc.path)) {
        doc.status = trf("Could not save %s", doc.path.c_str());
        return false;
    }
    doc.dirty = false;
    doc.status = trf("Saved %s", doc.info.path);
    return true;
}

bool TablesPanel::saveAll(std::string* status) {
    bool ok = true;
    for (Doc& doc : m_docs)
        if (doc.dirty) {
            ok = save(doc) && ok;
            if (status) *status = doc.status;
        }
    return ok;
}

bool TablesPanel::dirty() const {
    for (const Doc& doc : m_docs)
        if (doc.dirty) return true;
    return false;
}

bool TablesPanel::takeViewTableChanged() {
    bool changed = false;
    for (Doc& doc : m_docs) {
        changed = changed || doc.viewChanged;
        doc.viewChanged = false;
    }
    return changed;
}

void TablesPanel::rebuildIndex() {
    m_index.clear();
    for (const Doc& doc : m_docs)
        if (doc.data.rowType) m_index.push_back({doc.data.rowType, &doc.data.ids});
}

std::string TablesPanel::uniqueId(const Doc& doc, const std::string& base) const {
    auto taken = [&](const std::string& id) { return std::find(doc.data.ids.begin(), doc.data.ids.end(), id) != doc.data.ids.end(); };
    if (!taken(base)) return base;
    for (int n = 2;; ++n) {
        const std::string id = base + "_" + std::to_string(n);
        if (!taken(id)) return id;
    }
}

void TablesPanel::collectProblems(std::vector<std::pair<int, std::string>>& out) {
    std::vector<std::string> found;
    for (size_t d = 0; d < m_docs.size(); ++d) {
        Doc& doc = m_docs[d];
        if (!doc.data.rowType) continue;
        found.clear();
        for (size_t i = 0; i < doc.data.rows.size(); ++i)
            data::resolveRows(*doc.data.rowType, doc.data.rows[i].data(), m_index, &found,
                              std::string(doc.info.path) + " " + doc.data.ids[i] + ": ");
        for (size_t i = 0; i < doc.data.rows.size(); ++i)
            collectRequired(*doc.data.rowType, doc.data.rows[i].data(), std::string(doc.info.path) + " " + doc.data.ids[i] + ": ", found);
        for (std::string& text : found) out.emplace_back(static_cast<int>(d), std::move(text));
    }
}

void TablesPanel::drawContent(int which, void* owner, bool editing) {
    if (which < 0 || which >= docCount()) {
        ImGui::TextDisabled("%s", tr("Tables appear when the game code is loaded (MEALKIT_TABLE)."));
        return;
    }
    m_current = which;
    Doc& doc = m_docs[static_cast<size_t>(which)];
    if (!doc.data.rowType) {
        ImGui::TextDisabled("%s", tr("Waiting for the game code (values are kept)."));
        return;
    }
    drawDoc(doc, owner, editing);
    doc.undo.settle(ImGui::IsAnyItemActive(), [&] {
        data::TableText now;
        data::tableToText(doc.data, now);
        return now;
    });
}

void TablesPanel::revertDoc(int which) {
    Doc& doc = m_docs[static_cast<size_t>(which)];
    UndoStack<data::TableText> history = doc.undo;
    load(doc);
    doc.undo = std::move(history);
    doc.undo.mark();
}

void TablesPanel::step(int which, bool back) {
    if (which < 0 || which >= docCount()) return;
    Doc& doc = m_docs[static_cast<size_t>(which)];
    data::TableText text;
    if (!doc.data.rowType || !doc.undo.step(back, text)) return;
    const reflect::TypeInfo* rowType = doc.data.rowType;
    doc.data = {};
    std::string warnings;
    data::tableFromText(*rowType, text, doc.data, &warnings);
    doc.dirty = true;
    if ((doc.info.flags & reflect::kTableView) != 0) doc.viewChanged = true;
    doc.selected = -1;
    m_idRow = -1;
    rebuildIndex();
}

// The window between choosing a CSV and replacing the table: counts, what to check, and the changed cells
void TablesPanel::drawImportPreview(Doc& doc) {
    if (doc.csvOpen) {
        ImGui::OpenPopup(trl("Import CSV Preview###csvPreview"));
        doc.csvOpen = false;
    }
    const f32 em = ImGui::GetFontSize();
    ImGui::SetNextWindowSize(ImVec2(em * 52.0f, em * 30.0f), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::BeginPopupModal(trl("Import CSV Preview###csvPreview"), &open)) return;
    const data::CsvPreview& p = doc.csv;
    std::string rowsChanged;
    {
        std::vector<std::string> seen;
        for (const data::CsvChange& c : p.changed)
            if (std::find(seen.begin(), seen.end(), c.row) == seen.end()) seen.push_back(c.row);
        rowsChanged = std::to_string(seen.size());
    }
    ImGui::TextUnformatted(doc.csvPath.c_str());
    ImGui::TextWrapped("%s", trf("%zu cells changed in %s rows, %zu rows added, %zu rows removed, %zu rows the same", p.changed.size(), rowsChanged.c_str(), p.added.size(), p.removed.size(), p.sameRows).c_str());
    if (p.changed.empty() && p.added.empty() && p.removed.empty()) ImGui::TextDisabled("%s", tr("The file has the same values as the table."));
    if (!p.removed.empty()) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", tr("Rows that are not in the file are removed."));
    constexpr size_t kShown = 500;
    const f32 footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("##csvLists", ImVec2(0.0f, -footer))) {
        if (!p.notes.empty() && ImGui::CollapsingHeader(trf("Check before importing (%zu)", p.notes.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            for (const std::string& note : p.notes) ImGui::TextWrapped("- %s", note.c_str());
        if (!p.changed.empty() && ImGui::CollapsingHeader(trf("Changed cells (%zu)", p.changed.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen) &&
            ImGui::BeginTable("##csvChanged", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(tr("Row"));
            ImGui::TableSetupColumn(tr("Column"));
            ImGui::TableSetupColumn(tr("Now"));
            ImGui::TableSetupColumn(tr("In the file"));
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < p.changed.size() && i < kShown; ++i) {
                const data::CsvChange& c = p.changed[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(c.row.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(c.column.c_str());
                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", c.before.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(c.after.c_str());
            }
            ImGui::EndTable();
            if (p.changed.size() > kShown) ImGui::TextDisabled("%s", trf("%zu more not listed", p.changed.size() - kShown).c_str());
        }
        const auto list = [&](const std::vector<std::string>& ids, const char* title) {
            if (ids.empty() || !ImGui::CollapsingHeader(trf("%s (%zu)", title, ids.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;
            for (size_t i = 0; i < ids.size() && i < kShown; ++i) ImGui::BulletText("%s", ids[i].c_str());
            if (ids.size() > kShown) ImGui::TextDisabled("%s", trf("%zu more not listed", ids.size() - kShown).c_str());
        };
        list(p.added, tr("Added rows"));
        list(p.removed, tr("Removed rows"));
    }
    ImGui::EndChild();
    bool close = !open;
    if (ImGui::Button(tr("Import"))) {
        std::string error;
        data::TableData& t = doc.data;
        if (data::importTableCsv(t, doc.csvPath, &error)) {
            doc.selected = -1;
            doc.status = trf("Imported %s (%zu rows) - save to write the table", fileName(doc.csvPath).c_str(), t.rows.size());
            doc.dirty = true;
            if ((doc.info.flags & reflect::kTableView) != 0) doc.viewChanged = true;
            doc.undo.mark();
            rememberCsvPath(doc, doc.csvPath);
        } else {
            doc.status = trf("Could not import: %s", error.c_str());
        }
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) close = true;
    if (close) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void TablesPanel::drawDoc(Doc& doc, void* owner, bool editing) {
    (void)editing;
    data::TableData& t = doc.data;
    const bool view = (doc.info.flags & reflect::kTableView) != 0;
    auto changed = [&] {
        doc.dirty = true;
        if (view) doc.viewChanged = true;
        doc.undo.mark();
    };

    // ---- Toolbar
    ImGui::BeginDisabled(!doc.dirty);
    if (ImGui::Button(tr("Save"))) save(doc);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("Reload from File"))) { // undoable: unsaved edits come back with Undo
        UndoStack<data::TableText> history = doc.undo;
        load(doc);
        doc.undo = std::move(history);
        doc.undo.mark();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button(tr("Add Row"))) {
        const int at = doc.selected >= 0 ? doc.selected + 1 : static_cast<int>(t.rows.size());
        t.ids.insert(t.ids.begin() + at, uniqueId(doc, "row"));
        t.rows.insert(t.rows.begin() + at, reflect::Instance(*t.rowType));
        doc.selected = at;
        changed();
    }
    const bool hasRow = doc.selected >= 0 && doc.selected < static_cast<int>(t.rows.size());
    const bool viewChanged = m_rowFilter[0] != 0 || m_sorted; // moving rows only makes sense in file order
    ImGui::BeginDisabled(!hasRow);
    ImGui::SameLine();
    if (ImGui::Button(tr("Duplicate"))) {
        const size_t i = static_cast<size_t>(doc.selected);
        reflect::Instance copy(*t.rowType);
        t.rowType->copy(copy.data(), t.rows[i].data());
        t.ids.insert(t.ids.begin() + static_cast<std::ptrdiff_t>(i + 1), uniqueId(doc, t.ids[i]));
        t.rows.insert(t.rows.begin() + static_cast<std::ptrdiff_t>(i + 1), std::move(copy));
        doc.selected = static_cast<int>(i + 1);
        changed();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Delete"))) {
        const auto i = static_cast<std::ptrdiff_t>(doc.selected);
        t.ids.erase(t.ids.begin() + i);
        t.rows.erase(t.rows.begin() + i);
        doc.selected = std::min(doc.selected, static_cast<int>(t.rows.size()) - 1);
        changed();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##up", ImGuiDir_Up) && doc.selected > 0 && !viewChanged) {
        const size_t i = static_cast<size_t>(doc.selected);
        std::swap(t.ids[i], t.ids[i - 1]);
        std::swap(t.rows[i], t.rows[i - 1]);
        --doc.selected;
        changed();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down) && doc.selected + 1 < static_cast<int>(t.rows.size()) && !viewChanged) {
        const size_t i = static_cast<size_t>(doc.selected);
        std::swap(t.ids[i], t.ids[i + 1]);
        std::swap(t.rows[i], t.rows[i + 1]);
        ++doc.selected;
        changed();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    // Import replaces the whole table, so the changes are shown first and nothing happens until they are accepted
    const auto startImport = [&](const std::string& csv) {
        std::string error;
        if (data::previewTableCsv(t, csv, doc.csv, &error)) {
            doc.csvPath = csv;
            doc.csvOpen = true;
        } else {
            doc.status = trf("Could not import: %s", error.c_str());
        }
    };
    const std::string lastCsv = csvPathOf(doc);
    if (ImGui::Button(tr("Import CSV..."))) {
        const std::string csv = openFileDialog(owner, lastCsv.empty() ? "" : fileName(lastCsv), lastCsv.empty() ? m_projectDir : parentDirectory(lastCsv), kCsvFilters, 2);
        if (!csv.empty()) startImport(csv);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(lastCsv.empty() || !fileExists(lastCsv));
    if (ImGui::Button(tr("Reimport"))) startImport(lastCsv);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", lastCsv.empty() ? tr("Import or export a CSV first: the table remembers its file") : lastCsv.c_str());
    ImGui::SameLine();
    if (ImGui::Button(tr("Export CSV..."))) {
        const std::string base = fileName(doc.info.path);
        const std::string name = base.substr(0, base.rfind('.')) + ".csv";
        const std::string csv = saveFileDialog(owner, lastCsv.empty() ? name : fileName(lastCsv), lastCsv.empty() ? m_projectDir : parentDirectory(lastCsv), kCsvFilters, 2, "csv");
        if (!csv.empty()) {
            const bool written = data::exportTableCsv(t, csv);
            doc.status = written ? trf("Exported %s", csv.c_str()) : trf("Could not write %s", csv.c_str());
            if (written) rememberCsvPath(doc, csv);
        }
    }
    // The note moved to a tooltip: on a short bottom strip the rows need the room
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", view ? tr("Display table: saved changes apply while playing")
                                     : tr("Simulation table: used from the next play (content hash). Play saves it"));
    if (!doc.status.empty()) ImGui::TextDisabled("%s", doc.status.c_str());

    // ---- Cells
    FieldContext context{&m_index};
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
    ImGui::InputTextWithHint("##rowFilter", tr("Filter rows"), m_rowFilter, sizeof(m_rowFilter));
    // Bulk edit by percentage over the selected cells (a single value is set by pasting it over the range)
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
    ImGui::InputFloat("##scalePercent", &m_scalePercent, 0.0f, 0.0f, "%.4g%%");
    m_scalePercent = std::clamp(m_scalePercent, -100.0f, 10000.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tr("Percent to change the selected number cells by: +10 is ten percent more, -20 is twenty percent less"));
    ImGui::SameLine();
    ImGui::BeginDisabled(doc.anchorRow < 0);
    if (ImGui::Button(tr("Scale Selected"))) m_scaleRequested = true;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tr("Multiplies the number cells of the selected range (click a cell, shift+click another) by the percent. Other cells are left alone"));
    const bool rowSelected = doc.selected >= 0 && doc.selected < static_cast<int>(t.rows.size());
    const f32 detailHeight = rowSelected ? ImGui::GetContentRegionAvail().y * 0.35f : 0.0f;
    const int columns = 2 + static_cast<int>(doc.columns.size());
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit |
                                  ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate;
    if (columns <= 64 && ImGui::BeginTable("##rows", columns, flags, ImVec2(0.0f, -detailHeight))) {
        ImGui::TableSetupScrollFreeze(2, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 28.0f);
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        for (const std::string& c : doc.columns) {
            // Column names: for top-level fields their description (text declared by the game), inner paths (a.b, a[1]) as is. CSV column names are paths
            const reflect::FieldInfo* field = t.rowType->field(c.c_str());
            const std::string name = field ? trData(splitFieldLabel(field->label).name.c_str()) : c;
            const f32 fit = ImGui::CalcTextSize(name.c_str()).x + ImGui::GetStyle().CellPadding.x * 2.0f + ImGui::GetFontSize();
            ImGui::TableSetupColumn(name.c_str(), ImGuiTableColumnFlags_WidthFixed, std::clamp(fit, 110.0f, 200.0f)); // long labels are cut; the tooltip has them whole
        }
        // Headers show only the name; the description and the file path are in the tooltip
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column = 0; column < columns; ++column) {
            ImGui::TableSetColumnIndex(column);
            ImGui::TableHeader(ImGui::TableGetColumnName(column));
            if (column < 2 || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) continue;
            const std::string& path = doc.columns[static_cast<size_t>(column - 2)];
            const reflect::FieldInfo* field = t.rowType->field(path.c_str());
            const FieldLabel label = field ? splitFieldLabel(field->label) : FieldLabel{};
            if (label.tooltip.empty()) ImGui::SetTooltip("%s\n%s", ImGui::TableGetColumnName(column), path.c_str());
            else ImGui::SetTooltip("%s\n%s\n%s", ImGui::TableGetColumnName(column), trData(label.tooltip.c_str()), path.c_str());
        }
        // Rows to show: the filter over ids and values, then the clicked column. File order is unchanged
        std::vector<int> order;
        const ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
        const bool sorting = sort && sort->SpecsCount > 0;
        m_sorted = sorting;
        std::vector<reflect::TextFields> texts;
        if (m_rowFilter[0] || sorting) {
            texts.resize(t.rows.size());
            for (size_t i = 0; i < t.rows.size(); ++i) reflect::toText(*t.rowType, t.rows[i].data(), texts[i]);
        }
        const auto hasText = [this](const std::string& text) {
            const std::string needle = m_rowFilter;
            return std::search(text.begin(), text.end(), needle.begin(), needle.end(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }) != text.end();
        };
        for (int i = 0; i < static_cast<int>(t.rows.size()); ++i) {
            bool show = !m_rowFilter[0] || hasText(t.ids[static_cast<size_t>(i)]);
            if (!show)
                for (const auto& field : texts[static_cast<size_t>(i)]) show = show || hasText(field.second);
            if (show) order.push_back(i);
        }
        if (sorting) {
            const int column = sort->Specs[0].ColumnIndex;
            const bool ascending = sort->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            const auto cell = [&](int row) -> const std::string& {
                if (column <= 1) return t.ids[static_cast<size_t>(row)];
                return texts[static_cast<size_t>(row)][static_cast<size_t>(column - 2)].second;
            };
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                const std::string& x = ascending ? cell(a) : cell(b);
                const std::string& y = ascending ? cell(b) : cell(a);
                char* endX = nullptr;
                char* endY = nullptr;
                const f32 nx = std::strtof(x.c_str(), &endX), ny = std::strtof(y.c_str(), &endY);
                if (!x.empty() && !y.empty() && !*endX && !*endY) return nx < ny;
                return x < y;
            });
        }
        // Cell range (anchor to focus, in the order shown). Ctrl+C copies it as tab-separated text, Ctrl+V pastes from its top-left cell
        std::vector<int> position(t.rows.size(), -1);
        for (size_t s = 0; s < order.size(); ++s) position[static_cast<size_t>(order[s])] = static_cast<int>(s);
        const auto isShown = [&](int row) { return row >= 0 && row < static_cast<int>(position.size()) && position[static_cast<size_t>(row)] >= 0; };
        int rowLo = -1, rowHi = -1, colLo = 0, colHi = 0;
        if (isShown(doc.anchorRow) && isShown(doc.focusRow)) {
            rowLo = std::min(position[static_cast<size_t>(doc.anchorRow)], position[static_cast<size_t>(doc.focusRow)]);
            rowHi = std::max(position[static_cast<size_t>(doc.anchorRow)], position[static_cast<size_t>(doc.focusRow)]);
            colLo = std::min(doc.anchorCol, doc.focusCol);
            colHi = std::min(std::max(doc.anchorCol, doc.focusCol), static_cast<int>(doc.columns.size()));
        }
        const auto columnName = [&](int col) -> std::string { return col == 0 ? std::string("id") : doc.columns[static_cast<size_t>(col - 1)]; };
        if (m_scaleRequested) {
            m_scaleRequested = false;
            if (rowLo < 0) {
                doc.status = tr("Select cells first: click a cell, shift+click another");
            } else {
                const double factor = 1.0 + static_cast<double>(m_scalePercent) / 100.0;
                int scaled = 0, same = 0, failed = 0;
                std::string firstError;
                for (int s = rowLo; s <= rowHi; ++s) {
                    for (int col = std::max(colLo, 1); col <= colHi; ++col) {
                        std::string error;
                        switch (data::scaleCell(t, static_cast<size_t>(order[static_cast<size_t>(s)]), columnName(col), factor, &error)) {
                        case data::ScaleResult::Changed: ++scaled; break;
                        case data::ScaleResult::Unchanged: ++same; break;
                        case data::ScaleResult::Failed:
                            if (!failed) firstError = error;
                            ++failed;
                            break;
                        case data::ScaleResult::NotNumber: break;
                        }
                    }
                }
                if (scaled) changed();
                doc.status = trf("Scaled %d cells by %+.4g%%", scaled, static_cast<double>(m_scalePercent));
                if (same) doc.status += trf(" (%d did not change)", same);
                if (failed) doc.status += trf(" (%d not accepted, first: %s)", failed, firstError.c_str());
            }
        }
        if (rowLo >= 0 && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
                std::vector<std::vector<std::string>> cells;
                for (int s = rowLo; s <= rowHi; ++s) {
                    cells.emplace_back();
                    for (int col = colLo; col <= colHi; ++col) cells.back().push_back(data::cellText(t, static_cast<size_t>(order[static_cast<size_t>(s)]), columnName(col)));
                }
                ImGui::SetClipboardText(data::formatTsv(cells).c_str());
                doc.status = trf("Copied %d cells", (rowHi - rowLo + 1) * (colHi - colLo + 1));
            } else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
                const char* clip = ImGui::GetClipboardText();
                const std::vector<std::vector<std::string>> cells = data::parseTsv(clip ? clip : "");
                if (!cells.empty()) {
                    const bool single = cells.size() == 1 && cells[0].size() == 1; // one value fills the whole range, like a spreadsheet
                    size_t width = 0;
                    for (const auto& line : cells) width = std::max(width, line.size());
                    const int rowsToFill = single ? rowHi - rowLo + 1 : static_cast<int>(cells.size());
                    const int colsToFill = single ? colHi - colLo + 1 : static_cast<int>(width);
                    int done = 0, failed = 0, outside = 0;
                    std::string firstError;
                    for (int r = 0; r < rowsToFill; ++r) {
                        for (int c = 0; c < colsToFill; ++c) {
                            const int s = rowLo + r, col = colLo + c;
                            const size_t line = single ? 0 : static_cast<size_t>(r);
                            if (!single && static_cast<size_t>(c) >= cells[line].size()) continue; // a short line leaves the rest alone
                            if (s >= static_cast<int>(order.size()) || col > static_cast<int>(doc.columns.size())) {
                                ++outside;
                                continue;
                            }
                            std::string error;
                            if (data::setCellText(t, static_cast<size_t>(order[static_cast<size_t>(s)]), columnName(col), single ? cells[0][0] : cells[line][static_cast<size_t>(c)], &error)) {
                                ++done;
                            } else {
                                if (!failed) firstError = error;
                                ++failed;
                            }
                        }
                    }
                    if (done) changed();
                    doc.status = trf("Pasted %d cells", done);
                    if (outside) doc.status += trf(" (%d past the table's end skipped)", outside);
                    if (failed) doc.status += trf(" (%d not accepted, first: %s)", failed, firstError.c_str());
                    if (!single) { // the range becomes what was pasted
                        doc.focusRow = order[static_cast<size_t>(std::min(rowLo + rowsToFill, static_cast<int>(order.size())) - 1)];
                        doc.focusCol = std::min(colLo + colsToFill - 1, static_cast<int>(doc.columns.size()));
                    }
                }
            }
        }
        const auto inRange = [&](int row, int col) {
            return rowLo >= 0 && isShown(row) && position[static_cast<size_t>(row)] >= rowLo && position[static_cast<size_t>(row)] <= rowHi && col >= colLo && col <= colHi;
        };
        const auto clickCell = [&](int row, int col) { // click picks a cell, shift+click extends the range
            if (ImGui::GetIO().KeyShift && doc.anchorRow >= 0) {
                doc.focusRow = row;
                doc.focusCol = col;
            } else {
                doc.anchorRow = doc.focusRow = row;
                doc.anchorCol = doc.focusCol = col;
            }
        };
        std::vector<FieldLabel> attributes; // ranges, units and pickers of top-level fields also apply in cells
        for (const std::string& c : doc.columns) {
            const reflect::FieldInfo* field = t.rowType->field(c.c_str());
            attributes.push_back(field ? splitFieldLabel(field->label) : FieldLabel{});
        }
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(order.size()));
        if (m_scrollToRow >= 0) {
            const auto at = std::find(order.begin(), order.end(), m_scrollToRow);
            if (at != order.end()) clipper.IncludeItemByIndex(static_cast<int>(at - order.begin()));
        }
        while (clipper.Step()) {
            for (int shown = clipper.DisplayStart; shown < clipper.DisplayEnd; ++shown) {
                const int row = order[static_cast<size_t>(shown)];
                const size_t r = static_cast<size_t>(row);
                ImGui::TableNextRow();
                if (row == m_scrollToRow) {
                    ImGui::SetScrollHereY();
                    m_scrollToRow = -1;
                }
                ImGui::PushID(row);
                // The row type's validate: cells of the fields it names get a red frame (the [Problems] list has them too)
                std::vector<reflect::Problem> problems;
                if (t.rowType->validate) t.rowType->validate(t.rows[r].data(), problems);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::to_string(row).c_str(), doc.selected == row)) doc.selected = row;
                if (!problems.empty()) { // the row number shows every problem of the row, also those of hidden columns
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(230, 70, 60, 255), 0.0f, 0, 2.0f);
                    if (ImGui::IsItemHovered()) {
                        std::string all;
                        for (const reflect::Problem& p : problems) all += (all.empty() ? "" : "\n") + (p.field ? std::string(p.field) + ": " : std::string()) + p.text;
                        ImGui::SetTooltip("%s", all.c_str());
                    }
                }
                ImGui::TableNextColumn();
                if (inRange(row, 0)) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                // id: applied on Enter or clicking elsewhere (letters/digits/_/-/. , unique within the table)
                char temp[128];
                char* buffer = m_idRow == row ? m_idBuffer : temp;
                if (m_idRow != row) std::snprintf(temp, sizeof(temp), "%s", t.ids[r].c_str());
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##id", buffer, sizeof(m_idBuffer));
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clickCell(row, 0);
                if (ImGui::IsItemActivated()) {
                    m_idRow = row;
                    std::snprintf(m_idBuffer, sizeof(m_idBuffer), "%s", t.ids[r].c_str());
                }
                if (ImGui::IsItemDeactivated() && m_idRow == row) {
                    const std::string id = m_idBuffer;
                    m_idRow = -1;
                    if (id != t.ids[r]) {
                        if (!data::validRowId(id) || std::find(t.ids.begin(), t.ids.end(), id) != t.ids.end()) {
                            doc.status = trf("'%s' cannot be a row id (letters, digits, _ - . and unique in the table)", id.c_str());
                        } else {
                            t.ids[r] = id;
                            changed();
                        }
                    }
                }
                for (size_t c = 0; c < doc.columns.size(); ++c) {
                    const std::string& path = doc.columns[c];
                    ImGui::TableNextColumn();
                    if (inRange(row, static_cast<int>(c) + 1)) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                    const reflect::ValueInfo* value = nullptr;
                    void* data = nullptr;
                    if (!reflect::resolvePath(*t.rowType, t.rows[r].data(), path, value, data)) continue;
                    ImGui::PushID(path.c_str());
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (drawValueWidget("##v", path.c_str(), *value, data, &context, &attributes[c])) changed();
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clickCell(row, static_cast<int>(c) + 1);
                    if (value->kind == reflect::Kind::Float && attributes[c].hasRange) { // how far along its range, under the number
                        const f32 share = std::clamp((*static_cast<const f32*>(data) - attributes[c].min) / (attributes[c].max - attributes[c].min), 0.0f, 1.0f);
                        const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(lo.x, hi.y - 2.0f), ImVec2(lo.x + (hi.x - lo.x) * share, hi.y), ImGui::GetColorU32(ImGuiCol_PlotHistogram));
                    }
                    // A value outside the field's range=min..max gets a red frame (warn only: the value is kept, data::rowRangeWarnings logs it on load)
                    double shown = 0.0;
                    if (attributes[c].hasRange && data::valueOutOfRange(*value, data, attributes[c].min, attributes[c].max, &shown)) {
                        const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRect(lo, hi, IM_COL32(230, 70, 60, 255), 0.0f, 0, 2.0f);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", trf("%.6g is outside the range %.6g..%.6g", shown, static_cast<double>(attributes[c].min),
                                                        static_cast<double>(attributes[c].max)).c_str());
                    }
                    std::string problem; // validate problems about this column's top-level field
                    const std::string top = path.substr(0, path.find_first_of(".["));
                    for (const reflect::Problem& p : problems)
                        if (p.field && top == p.field) problem += (problem.empty() ? "" : "\n") + p.text;
                    if (!problem.empty()) {
                        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(230, 70, 60, 255), 0.0f, 0, 2.0f);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", problem.c_str());
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    drawImportPreview(doc);
    // Long values are easier to edit in the chosen row's own list
    if (rowSelected && doc.selected < static_cast<int>(t.rows.size())) {
        if (ImGui::BeginChild("##rowDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            ImGui::TextDisabled("%s", t.ids[static_cast<size_t>(doc.selected)].c_str());
            if (drawFields(*t.rowType, t.rows[static_cast<size_t>(doc.selected)].data(), &context)) changed();
        }
        ImGui::EndChild();
    }
}

} // namespace mealkit::editor
