#include "mealkit/editor/TablesPanel.h"

#include "mealkit/core/Log.h"
#include "mealkit/editor/FieldEditor.h"
#include "mealkit/editor/Strings.h"
#include "mealkit/platform/FileDialog.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>

namespace mealkit::editor {
namespace {

const FileFilter kCsvFilters[] = {{"CSV (*.csv)", "*.csv"}, {"All files (*.*)", "*.*"}};

std::string backslashes(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    return path;
}

} // namespace

void TablesPanel::setProjectDirectory(const std::string& directory) {
    m_projectDir = directory;
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
    if (ImGui::ArrowButton("##up", ImGuiDir_Up) && doc.selected > 0) {
        const size_t i = static_cast<size_t>(doc.selected);
        std::swap(t.ids[i], t.ids[i - 1]);
        std::swap(t.rows[i], t.rows[i - 1]);
        --doc.selected;
        changed();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down) && doc.selected + 1 < static_cast<int>(t.rows.size())) {
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
    if (ImGui::Button(tr("Import CSV..."))) {
        const std::string csv = openFileDialog(owner, "", m_projectDir, kCsvFilters, 2);
        std::string error;
        if (!csv.empty()) {
            if (data::importTableCsv(t, csv, &error)) {
                doc.selected = -1;
                doc.status = trf("Imported %s (%zu rows) - save to write the table", fileName(csv).c_str(), t.rows.size());
                changed();
            } else {
                doc.status = trf("Could not import: %s", error.c_str());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Export CSV..."))) {
        const std::string base = fileName(doc.info.path);
        const std::string name = base.substr(0, base.rfind('.')) + ".csv";
        const std::string csv = saveFileDialog(owner, name, m_projectDir, kCsvFilters, 2, "csv");
        if (!csv.empty())
            doc.status = data::exportTableCsv(t, csv) ? trf("Exported %s", csv.c_str()) : trf("Could not write %s", csv.c_str());
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
    const int columns = 2 + static_cast<int>(doc.columns.size());
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
    if (columns <= 64 && ImGui::BeginTable("##rows", columns, flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupScrollFreeze(2, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
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
        std::vector<FieldLabel> attributes; // ranges, units and pickers of top-level fields also apply in cells
        for (const std::string& c : doc.columns) {
            const reflect::FieldInfo* field = t.rowType->field(c.c_str());
            attributes.push_back(field ? splitFieldLabel(field->label) : FieldLabel{});
        }
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(t.rows.size()));
        if (m_scrollToRow >= 0) {
            clipper.IncludeItemByIndex(m_scrollToRow);
        }
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const size_t r = static_cast<size_t>(row);
                ImGui::TableNextRow();
                if (row == m_scrollToRow) {
                    ImGui::SetScrollHereY();
                    m_scrollToRow = -1;
                }
                ImGui::PushID(row);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::to_string(row).c_str(), doc.selected == row)) doc.selected = row;
                ImGui::TableNextColumn();
                // id: applied on Enter or clicking elsewhere (letters/digits/_/-/. , unique within the table)
                char temp[128];
                char* buffer = m_idRow == row ? m_idBuffer : temp;
                if (m_idRow != row) strncpy_s(temp, t.ids[r].c_str(), _TRUNCATE);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##id", buffer, sizeof(m_idBuffer));
                if (ImGui::IsItemActivated()) {
                    m_idRow = row;
                    strncpy_s(m_idBuffer, t.ids[r].c_str(), _TRUNCATE);
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
                    const reflect::ValueInfo* value = nullptr;
                    void* data = nullptr;
                    if (!reflect::resolvePath(*t.rowType, t.rows[r].data(), path, value, data)) continue;
                    ImGui::PushID(path.c_str());
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (drawValueWidget("##v", path.c_str(), *value, data, &context, &attributes[c])) changed();
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

} // namespace mealkit::editor
