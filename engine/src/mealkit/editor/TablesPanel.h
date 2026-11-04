#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/data/Tables.h"
#include "mealkit/editor/UndoStack.h"

#include <string>
#include <utility>
#include <vector>

// Table pages of the editor [Data] window: edits definition data tables registered by the game with MEALKIT_TABLE (assets/tables/*.table) like a spreadsheet.
// Add/duplicate/delete/reorder rows, field widgets per cell (dropdowns for row references), missing id reference warnings, CSV import/export, save/reload.
// Table value instances are of the game DLL's types, so they're kept as text before unloading the DLL and rebuilt with the new type (same as tuning).
namespace mealkit::editor {

class TablesPanel {
public:
    // Tables that were already opened (engine tables come before the project) are read again from the project's files.
    void setProjectDirectory(const std::string& directory);
    // Table list of the game DLL (nullptr before unloading the DLL). Read from files when first received.
    void setTables(const reflect::TableList* tables);
    // Tables registered by the engine (sound events etc.). Shown together with game tables.
    void setEngineTables(const reflect::TableList& tables);

    // owner: window for file dialogs (Window::nativeHandle)
    void drawContent(int which, void* owner, bool editing);
    int docCount() const { return static_cast<int>(m_docs.size()); }
    std::string docName(int which) const { return m_docs[static_cast<size_t>(which)].info.path; }
    bool docDirty(int which) const { return m_docs[static_cast<size_t>(which)].dirty; }
    const std::vector<std::string>& docIds(int which) const { return m_docs[static_cast<size_t>(which)].data.ids; }
    void selectRow(int which, int row) { m_docs[static_cast<size_t>(which)].selected = row; m_scrollToRow = row; }
    int current() const { return m_current; }
    void setCurrent(int which) { m_current = which; }
    bool canStep(int which, bool back) const { return which >= 0 && which < docCount() && m_docs[static_cast<size_t>(which)].undo.canStep(back); }
    void step(int which, bool back);
    // Saves all edited tables (before play starts: the game reads the files). false on failure.
    bool saveAll(std::string* status);
    bool dirty() const;
    // true once if any view-only table (kTableView) was edited (saved so it's reread immediately during play)
    bool takeViewTableChanged();

    // For row reference dropdowns and checks: row ids of the currently open tables
    const data::TableIndexList& index() const { return m_index; }
    // Missing row references inside the tables: document index and "<table> <row>: <path> -> <id> (table)" ([Problems] window)
    void collectProblems(std::vector<std::pair<int, std::string>>& out);

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
        UndoStack<data::TableText> undo;
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
    char m_idBuffer[128] = {};
    int m_idRow = -1;
    int m_scrollToRow = -1;
};

} // namespace mealkit::editor
