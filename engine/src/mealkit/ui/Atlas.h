#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit::render {
class Renderer;
}

// UI 글꼴과 이미지 (mealkit/ui/). 렌더러의 UI 아틀라스 두 장에 올린다(render::UiAtlas).
//   글꼴: assets/fonts의 .ttf·.otf를 파일 이름으로 찾는다("Pretendard-Medium"). stb_truetype으로 쓰는 글자만 실행 중에 SDF로 굽는다.
//         굵기·기울임마다 글꼴 파일 하나. 한 크기(kBakePixels)로 구워 모든 크기에 쓴다.
//   이미지: assets 기준 PNG (stb_image). 처음 쓸 때 읽어 이미지 아틀라스에 넣는다.
// 아틀라스가 차면 비우고 다시 굽는다(경고). 화면 쪽 전용이라 결정론과 무관하다.
namespace mealkit::ui {

inline constexpr f32 kBakePixels = 40.0f; // SDF를 구울 글자 크기 (em 픽셀)
inline constexpr i32 kSdfPadding = 6;     // 글자 둘레 여백 (구운 픽셀)

struct Glyph {
    bool valid = false;  // 글꼴에 있는가 (없으면 다른 글꼴에서 찾는다)
    f32 advance = 0.0f;  // em 1당 (크기를 곱해 쓴다)
    f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; // 기준선 기준 상자, em 1당 (y는 아래가 +), 여백 포함
    f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0; // 아틀라스 좌표 (빈 글자는 0)
};

struct FontMetrics {
    f32 ascent = 0.8f, descent = 0.2f, lineGap = 0.0f; // em 1당 (descent는 양수)
};

struct ImageInfo {
    bool valid = false;
    u32 width = 0, height = 0; // 원본 픽셀
    f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};

class Atlas {
public:
    Atlas();
    ~Atlas();
    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;

    void setRenderer(render::Renderer* renderer); // 아틀라스를 올릴 렌더러 (없으면 굽기만 한다: 배치 시험)

    // 글꼴 이름 → 번호 (없으면 첫 글꼴, 글꼴이 하나도 없으면 -1)
    i32 font(const std::string& name);
    const FontMetrics& metrics(i32 font);
    // 이 글꼴에 없으면 다른 글꼴에서 찾는다 (fontOut이 바뀐다)
    Glyph glyph(i32& fontInOut, u32 codepoint);
    f32 kerning(i32 font, u32 a, u32 b); // em 1당
    f32 sdfUnitsPerPixel() const;        // 구운 SDF에서 값 1(0..1)이 몇 구운 픽셀인가

    ImageInfo image(const std::string& assetPath);

    // 파일이 바뀌었으면 다시 읽는다 (0.25초마다 부른다)
    void reloadChanged();
    u32 glyphCount() const;

private:
    struct Impl;
    Impl* m;
};

} // namespace mealkit::ui
