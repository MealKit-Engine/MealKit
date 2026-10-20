#pragma once

#include "mealkit/core/Types.h"
#include "mealkit/ui/Atlas.h"

#include <string>
#include <vector>

// UI 글자 셰이핑 (ui::System 안에서만 쓴다): UTF-8 → 글자, 스타일 구간 태그(<1>…</1>), 커닝, 줄 나누기, 말줄임.
// 결과는 논리 단위다. 그리기는 Ui.cpp가 한다.
namespace mealkit::ui {

struct Node;

struct TextRun { // 글자 하나에 붙는 스타일
    i32 font = -1;
    f32 size = 24.0f;
    f32 letterSpacing = 0.0f;
    bool underline = false, strike = false;
    i32 span = -1; // Node::spans 번호 (색), 없으면 노드 색
};

struct ShapedGlyph {
    u32 cp = 0;
    u16 run = 0;
    i32 font = -1;
    f32 advance = 0.0f; // 논리 단위 (자간 포함)
    Glyph glyph;
};

struct ShapedLine {
    u32 begin = 0, end = 0; // 글자 범위 (end 앞의 공백은 너비에 넣지 않는다)
    f32 width = 0.0f;
    f32 ascent = 0.0f, descent = 0.0f, height = 0.0f;
};

struct Shaped {
    std::vector<TextRun> runs;
    std::vector<ShapedGlyph> glyphs;
    std::vector<ShapedLine> lines;
    f32 width = 0.0f, height = 0.0f;
};

std::vector<u32> decodeUtf8(const std::string& s);
// 태그(<1>…</1>)를 스타일로 풀고, 글자마다 글꼴·크기·폭을 정한다
void shapeGlyphs(const Node& d, const std::string& text, Atlas& atlas, Shaped& out);
// 줄 나누기 (구간 경계를 넘어 한 번에). wrapWidth < 0이면 줄바꿈 글자에서만. maxHeight < 0이면 높이 제한 없음
void breakLines(const Node& d, Shaped& s, f32 wrapWidth, f32 maxHeight, Atlas& atlas);

} // namespace mealkit::ui
