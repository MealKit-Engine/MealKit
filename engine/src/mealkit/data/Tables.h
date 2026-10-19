#pragma once

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"

#include <string>
#include <vector>

// 정의 데이터 표 파일 (assets/tables/*.table, mealkit/sim/Table.h). 게임 app/·에디터·콘텐츠 해시와 엔진 표(입력·소리·연출)가 같은 함수로 읽는다.
// data/는 게임 경계(game/)보다 아래 계층이다 (docs/CODING_MODEL.md "엔진 폴더 사이 의존").
//   # MealKit table (PropKind) ...
//   row crate
//   density 200
//   size 0.28 0.64
//   row barrel
//   ...
// 행 순서 = 파일 순서 = 행 번호. 행 id는 영문자·숫자·_·-·. 이고 표 안에서 겹치지 않는다(겹치면 뒤의 행을 버린다).
namespace mealkit::data {

// 표 파일 내용: 형식을 몰라도 다룬다 (행 id + 그 행의 "경로 값" 줄)
struct TableText {
    std::vector<std::string> ids;
    std::vector<reflect::TextFields> rows;
};
bool readTableText(const std::string& path, TableText& out, std::string* warnings = nullptr);
bool writeTableText(const std::string& path, const TableText& table, const char* rowTypeName);
bool validRowId(const std::string& id);
// assets 기준 경로 → 파일 (findAsset("assets/" + path)). 없으면 빈 문자열.
std::string findTableFile(const char* assetPath);

// 형식 없이 다루는 표 (에디터, 콘텐츠 해시): 행마다 행 타입 인스턴스
struct TableData {
    const reflect::TypeInfo* rowType = nullptr;
    std::vector<std::string> ids;
    std::vector<reflect::Instance> rows;
};
void tableToText(const TableData& table, TableText& out);
// 줄마다 행 타입으로 읽는다. 모르는 필드는 warnings에.
void tableFromText(const reflect::TypeInfo& rowType, const TableText& text, TableData& out,
                   std::string* warnings = nullptr);
bool loadTableData(const reflect::TypeInfo& rowType, const std::string& path, TableData& out,
                   std::string* warnings = nullptr);
bool saveTableData(const TableData& table, const std::string& path);

// CSV (Google Sheets·Excel): 첫 줄은 "id"와 필드 경로(table 파일과 같은 이름), 한 줄이 한 행. 가져오기는 표를 CSV 내용으로 바꾼다(한쪽 방향).
bool exportTableCsv(const TableData& table, const std::string& path);
bool importTableCsv(TableData& table, const std::string& path, std::string* error);

// 행 참조(RowRef) 풀기: data 안의 RowRef마다 가리키는 표(행 타입이 같은 표)에서 id를 찾아 행 번호를 넣는다.
// 없는 id는 missing에 "<where><경로> -> <id> (표 이름)" 한 줄. 빈 id는 참조 없음.
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

// 시뮬레이션 표(kTableView가 아닌 것)를 읽어 id와 행 값(MEALKIT_VIEW_FIELD 제외)을 해시에 넣는다 (콘텐츠 해시).
void hashTables(const reflect::TableList& tables, Hasher& h);

// ---------------------------------------------------------------- 형식을 아는 쪽 (게임 app/)

// MEALKIT_TABLE로 등록한 경로 (이 모듈의 등록 목록에서 행 타입으로 찾는다). 없으면 nullptr.
template <class Row>
const char* tablePath(const reflect::TableList& list = reflect::moduleTables()) {
    for (const reflect::TableInfo& t : list)
        if (t.rowType == &reflect::typeOf<Row>()) return t.path;
    return nullptr;
}

// 표를 읽는다 (등록한 경로, assetPath를 주면 그것). 파일이 없으면 빈 표와 경고.
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
