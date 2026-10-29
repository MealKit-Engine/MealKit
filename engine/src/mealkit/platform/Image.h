#pragma once

#include "mealkit/core/Types.h"

#include <string>
#include <vector>

// Reading/writing image files (the editor's project icon). The Windows implementation is WIC (PNG, JPEG, BMP, ICO etc.); other OSes with the stage 5 editor port.
namespace mealkit {

struct Image {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba; // RGBA8 in row order (non-premultiplied alpha)
};

// If the long side is larger than maxSize, scaled down keeping the aspect ratio. For ICO, the largest image is used.
bool loadImage(const std::string& path, u32 maxSize, Image& out);
bool loadImageResource(int resourceId, u32 maxSize, Image& out); // RCDATA resource in the executable
bool savePng(const std::string& path, const Image& image);

} // namespace mealkit
