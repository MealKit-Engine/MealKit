#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/game/Tables.h"

#include <string>
#include <vector>

// 에디터 [표] 패널: 게임이 MEALKIT_TABLE로 등록한 정의 데이터 표(assets/tables/*.table)를 스프레드시트처럼 고친다.
// 행 추가·복제·삭제·순서, 칸마다 필드 위젯(행 참조는 드롭다운), 없는 id 참조 경고, CSV 가져오기·내보내기, 저장·다시 읽기.
// 표 값의 인스턴스는 게임 DLL의 타입이라, DLL을 내리기 전에 텍스트로 보존했다가 새 타입으로 다시 만든다(튜닝과 같은 방식).
namespace mealkit::editor {

class TablesPanel {
public:
    void setProjectDirectory(const std::string& directory) { m_projectDir = directory; }
    // 게임 DLL의 표 목록 (DLL을 내리기 전에 nullptr). 처음 받으면 파일에서 읽는다.
    void setTables(const reflect::TableList* tables);
    // 엔진이 등록한 표 (소리 이벤트 등). 게임 표와 같이 보인다.
    void setEngineTables(const reflect::TableList& tables) { m_engineTables = tables; }

    // owner: 파일 대화상자의 창 (Window::nativeHandle), open: 창의 닫기 버튼
    void draw(void* owner, bool editing, bool* open);
    // 고친 표를 모두 저장한다 (플레이 시작 전: 게임은 파일을 읽는다). 실패하면 false.
    bool saveAll(std::string* status);
    bool dirty() const;
    // 화면 전용 표(kTableView) 중 고친 것이 있으면 한 번 true (플레이 중 바로 다시 읽게 저장한다)
    bool takeViewTableChanged();

    // 행 참조 드롭다운과 검사용: 지금 열린 표들의 행 id
    const game::TableIndexList& index() const { return m_index; }
    // 표·튜닝·씬 물체 필드의 없는 행 참조 ("<어디> <경로> -> <id> (표)")
    const std::vector<std::string>& problems() const { return m_problems; }
    // 패널 밖의 데이터(튜닝, 씬 물체)도 검사하도록 넣는다. 다음 draw에서 목록이 갱신된다.
    void setExtraProblems(std::vector<std::string> problems) { m_extraProblems = std::move(problems); }

private:
    struct Doc {
        reflect::TableInfo info{};
        std::string path; // 절대 경로
        game::TableData data;
        game::TableText backup; // DLL을 바꿔 끼울 때
        std::vector<std::string> columns; // 칸 경로 (기본값 행의 toText 순서)
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
    game::TableIndexList m_index;
    std::vector<std::string> m_problems;
    std::vector<std::string> m_extraProblems;
    char m_idBuffer[128] = {};
    int m_idRow = -1;
};

} // namespace mealkit::editor
