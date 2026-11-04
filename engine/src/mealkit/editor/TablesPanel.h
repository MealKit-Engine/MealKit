#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/data/Tables.h"

#include <string>
#include <vector>

// Editor [Tables] panel: edits definition data tables registered by the game with MEALKIT_TABLE (assets/tables/*.table) like a spreadsheet.
// Add/duplicate/delete/reorder rows, field widgets per cell (dropdowns for row references), missing id reference warnings, CSV import/export, save/reload.
// Table value instances are of the game DLL's types, so they're kept as text before unloading the DLL and rebuilt with the new type (same as tuning).
namespace mealkit::editor {

class TablesPanel {
public:
    static constexpr const char* kWindow = "Tables###Tables";
    // Tables that were already opened (engine tables come before the project) are read again from the project's files.
    void setProjectDirectory(const std::string& directory);
    // Table list of the game DLL (nullptr before unloading the DLL). Read from files when first received.
    void setTables(const reflect::TableList* tables);
    // Tables registered by the engine (sound events etc.). Shown together with game tables.
    void setEngineTables(const reflect::TableList& tables);

    // owner: window for file dialogs (Window::nativeHandle), open: the window's close button
    void draw(void* owner, bool editing, bool* open);
    // Saves all edited tables (before play starts: the game reads the files). false on failure.
    bool saveAll(std::string* status);
    bool dirty() const;
    // true once if any view-only table (kTableView) was edited (saved so it's reread immediately during play)
    bool takeViewTableChanged();

    // For row reference dropdowns and checks: row ids of the currently open tables
    const data::TableIndexList& index() const { return m_index; }
    // Missing row references in tables/tuning/scene object fields ("<where> <path> -> <id> (table)")
    const std::vector<std::string>& problems() const { return m_problems; }
    // Adds data outside the panel (tuning, scene objects) to be checked too. The list updates on the next draw.
    void setExtraProblems(std::vector<std::string> problems) { m_extraProblems = std::move(problems); }

private:
    struct Doc {
        reflect::TableInfo info{};
        std::string path; // absolute path
        data::TableData data;
        data::TableText backup; // when swapping the DLL
        std::vector<std::string> columns; // cell paths (toText order of the default row)
        bool dirty = false;
        bool viewChanged = false;
        int selected = -1;
        std::string status;
    };

    void syncDocs(const std::vector<reflect::TableInfo>& infos);
    void load(Doc& doc);
    bool save(Doc& doc);
    void rebuildIndex();
    void drawDoc(Doc& doc, void* owner, bool editing);
    std::string uniqueId(const Doc& doc, const std::string& base) const;

    std::string m_projectDir;
    reflect::TableList m_engineTables;
    std::vector<Doc> m_docs;
    int m_current = 0;
    data::TableIndexList m_index;
    std::vector<std::string> m_problems;
    std::vector<std::string> m_extraProblems;
    char m_idBuffer[128] = {};
    int m_idRow = -1;
};

} // namespace mealkit::editor
