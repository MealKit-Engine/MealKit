#pragma once

#include "mealkit/core/Types.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 게임 문자열 표 (docs/CODING_MODEL.md "게임 글자와 언어"): 게임 코드·UI는 글자 대신 키를 쓴다.
//   assets/strings/ko.txt, en.txt ...   "키 = 문자열" 한 줄씩 (# 주석, 줄바꿈은 \n, 역슬래시는 \\)
//     lobby.waiting = 호스트를 기다리는 중 ({count}명 대기)
//   ctx.strings.get("lobby.waiting"), ctx.strings.format("lobby.waiting", {{"count", "3"}})
// 고른 언어에 없는 키는 기본 언어(프로젝트 설정 language) 표에서, 거기에도 없으면 키 이름을 그대로 보여 주고 키마다 한 번 경고한다.
// 여러 스타일 글자는 번호 태그로 쓴다: "점수 <1>{score}</1>점" (태그는 UI가 해석한다, 문자열 표는 그대로 돌려준다).
// 표에 있는 글자가 모두 assets/fonts의 글꼴에 있는지 빌드 때 검사한다(mealkit_stringcheck). 콘텐츠 해시와 무관하다.
namespace mealkit::loc {

inline constexpr const char* kStringsFolder = "strings"; // assets 기준

class Strings {
public:
    Strings();
    ~Strings();
    Strings(const Strings&) = delete;
    Strings& operator=(const Strings&) = delete;

    // language: 보여 줄 언어(파일 이름, "ko"), fallback: 기본 언어. 파일을 읽는다. update가 0.25초마다 파일 시각을 보고 다시 읽는다.
    void load(std::string language, std::string fallback);
    void update(f64 dt);
    const std::string& language() const;
    // assets/strings에 있는 언어들 (설정 화면의 언어 고르기)
    std::vector<std::string> languages() const;

    // 키의 문자열. 없으면 키 이름 (경고 한 번). 파일을 다시 읽을 때까지 유효하다 (보관하지 않는다)
    const std::string& get(std::string_view key) const;
    bool has(std::string_view key) const;
    // {이름} 자리를 값으로 바꾼다. 없는 이름은 그대로 둔다
    std::string format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> args) const;

private:
    struct Impl;
    Impl* m;
};

// "키 = 문자열" 파일 한 개를 읽는다 (도구·검사도 같은 함수). 줄 번호는 1부터. 형식이 틀린 줄은 errors에.
struct StringEntry {
    std::string key;
    std::string value;
    u32 line = 0;
};
bool readStringsFile(const std::string& path, std::vector<StringEntry>& out, std::string* errors = nullptr);
bool validStringKey(std::string_view key); // 영문자·숫자·._-, 첫 글자는 영문자

} // namespace mealkit::loc
