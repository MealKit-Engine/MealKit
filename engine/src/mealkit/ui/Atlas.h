#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit::render {
class Renderer;
}

// UI fonts and images (mealkit/ui/). Uploaded to the renderer's two UI atlases (render::UiAtlas).
//   Fonts: .ttf/.otf in assets/fonts found by file name ("Pretendard-Medium"). Only glyphs in use are baked to SDF at runtime with stb_truetype.
//         One font file per weight/italic. Baked at one size (kBakePixels) and used for all sizes.
//   Images: PNGs relative to assets (stb_image). Read on first use and put into the image atlas.
// When an atlas fills up, it's cleared and rebaked (warning). View-side only, so unrelated to determinism.
namespace mealkit::ui {

inline constexpr f32 kBakePixels = 40.0f; // glyph size for baking SDFs (em pixels)
inline constexpr i32 kSdfPadding = 6;     // padding around glyphs (baked pixels)

struct Glyph {
    bool valid = false;  // whether it's in the font (otherwise looked up in other fonts)
    f32 advance = 0.0f;  // per em (multiplied by size)
    f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; // box relative to the baseline, per em (y is + downward), including padding
    f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0; // atlas coordinates (0 for empty glyphs)
};

struct FontMetrics {
    f32 ascent = 0.8f, descent = 0.2f, lineGap = 0.0f; // per em (descent is positive)
};

struct ImageInfo {
    bool valid = false;
    u32 width = 0, height = 0; // source pixels
    f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};

class Atlas {
public:
    Atlas();
    ~Atlas();
    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;

    void setRenderer(render::Renderer* renderer); // renderer to upload atlases to (without one, only bakes: layout tests)

    // Font name → index (first font if missing, -1 if there are no fonts at all)
    i32 font(const std::string& name);
    const FontMetrics& metrics(i32 font);
    // If not in this font, looks in other fonts (fontOut changes)
    Glyph glyph(i32& fontInOut, u32 codepoint);
    f32 kerning(i32 font, u32 a, u32 b); // per em
    f32 sdfUnitsPerPixel() const;        // how many baked pixels a value of 1 (0..1) is in the baked SDF

    ImageInfo image(const std::string& assetPath);

    // Rereads if files changed (called every 0.25 s)
    void reloadChanged();
    u32 glyphCount() const;

private:
    struct Impl;
    Impl* m;
};

} // namespace mealkit::ui
