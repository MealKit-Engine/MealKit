#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

// 에디터용 그림 파일 읽기·쓰기 (Windows WIC: PNG, JPEG, BMP, ICO 등). 프로젝트 아이콘에 쓴다.
namespace mealkit::editor {

struct Image {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba; // 줄 순서대로 RGBA8 (곱하지 않은 알파)
};

// 긴 변이 maxSize보다 크면 비율을 지켜 줄인다. ICO는 가장 큰 그림을 쓴다.
bool loadImage(const std::string& path, u32 maxSize, Image& out);
bool loadImageResource(int resourceId, u32 maxSize, Image& out); // 실행 파일의 RCDATA 리소스
bool savePng(const std::string& path, const Image& image);

} // namespace mealkit::editor
