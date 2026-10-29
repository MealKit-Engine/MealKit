#include "mealkit/ui/Atlas.h"

#include "mealkit/core/Log.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/render/Renderer.h"

#include "stb_image.h"
#include "stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace mealkit::ui {
namespace {

constexpr u32 kSize = render::kUiAtlasSize;
constexpr f32 kOnEdge = 128.0f;
constexpr f32 kDistScale = kOnEdge / static_cast<f32>(kSdfPadding); // SDF value per baked pixel

// Simple row-based rectangle packing (UI glyphs/images are similar in size)
struct Shelves {
    struct Shelf {
        u32 y, height, x;
    };
    std::vector<Shelf> shelves;
    u32 top = 0;

    void clear() {
        shelves.clear();
        top = 0;
    }
    bool place(u32 w, u32 h, u32& outX, u32& outY) {
        w += 1; // 1 pixel gap from neighbors (linear filtering)
        h += 1;
        for (Shelf& s : shelves)
            if (h <= s.height && h * 4 >= s.height * 3 && s.x + w <= kSize) {
                outX = s.x;
                outY = s.y;
                s.x += w;
                return true;
            }
        if (top + h > kSize || w > kSize) return false;
        shelves.push_back({top, h, w});
        outX = 0;
        outY = top;
        top += h;
        return true;
    }
};

struct FontFile {
    std::string name, path;
    unsigned long long time = 0;
    std::vector<u8> data;
    stbtt_fontinfo info{};
    f32 scale = 0.0f; // font units → baked pixels
    FontMetrics metrics;
};

struct Image {
    std::string path;
    unsigned long long time = 0;
    ImageInfo info;
};

} // namespace

struct Atlas::Impl {
    render::Renderer* renderer = nullptr;
    bool fontsScanned = false;
    std::vector<FontFile> fonts;
    std::unordered_map<u64, Glyph> glyphs;
    Shelves glyphShelves, imageShelves;
    std::vector<Image> images;
    Glyph none;
    ImageInfo noImage;
    FontMetrics defaultMetrics;

    void scanFonts() {
        fontsScanned = true;
        fonts.clear();
        const std::string dir = findAsset("assets/fonts");
        if (dir.empty()) return;
        for (const char* ext : {".ttf", ".otf", ".ttc"})
            for (const std::string& file : listFiles(dir, ext)) {
                FontFile f;
                f.name = file.substr(0, file.size() - 4);
                f.path = dir + "/" + file;
                if (!loadFont(f)) continue;
                fonts.push_back(std::move(f));
            }
        if (fonts.empty()) MEALKIT_WARN("UI: no fonts in assets/fonts - text is not drawn");
    }

    bool loadFont(FontFile& f) {
        std::string bytes;
        if (!readTextFile(f.path, bytes)) return false;
        f.data.assign(bytes.begin(), bytes.end());
        f.time = fileWriteTime(f.path);
        const int offset = stbtt_GetFontOffsetForIndex(f.data.data(), 0);
        if (offset < 0 || !stbtt_InitFont(&f.info, f.data.data(), offset)) {
            MEALKIT_WARN("UI: cannot read font %s", f.path.c_str());
            return false;
        }
        f.scale = stbtt_ScaleForMappingEmToPixels(&f.info, kBakePixels);
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &lineGap);
        const f32 em = f.scale / kBakePixels;
        f.metrics = {static_cast<f32>(ascent) * em, static_cast<f32>(-descent) * em, static_cast<f32>(lineGap) * em};
        return true;
    }

    void resetGlyphs() {
        glyphs.clear();
        glyphShelves.clear();
    }

    // Flattens outlines into line segments and computes the SDF directly. stbtt_GetGlyphSDF misses cubic curves (CFF/.otf fonts), so round strokes vanish.
    static void flatten(const stbtt_vertex* v, int count, f32 scale, std::vector<f32>& seg) {
        seg.clear();
        f32 px = 0, py = 0;
        auto line = [&](f32 x, f32 y) {
            seg.insert(seg.end(), {px, py, x, y});
            px = x;
            py = y;
        };
        for (int i = 0; i < count; ++i) {
            const f32 x = static_cast<f32>(v[i].x) * scale, y = -static_cast<f32>(v[i].y) * scale; // y is + downward
            if (v[i].type == STBTT_vmove) {
                px = x;
                py = y;
            } else if (v[i].type == STBTT_vline) {
                line(x, y);
            } else if (v[i].type == STBTT_vcurve) {
                const f32 cx = static_cast<f32>(v[i].cx) * scale, cy = -static_cast<f32>(v[i].cy) * scale;
                const f32 sx = px, sy = py;
                constexpr int kSteps = 8;
                for (int k = 1; k <= kSteps; ++k) {
                    const f32 t = static_cast<f32>(k) / kSteps, u = 1.0f - t;
                    line(u * u * sx + 2 * u * t * cx + t * t * x, u * u * sy + 2 * u * t * cy + t * t * y);
                }
            } else if (v[i].type == STBTT_vcubic) {
                const f32 c0x = static_cast<f32>(v[i].cx) * scale, c0y = -static_cast<f32>(v[i].cy) * scale;
                const f32 c1x = static_cast<f32>(v[i].cx1) * scale, c1y = -static_cast<f32>(v[i].cy1) * scale;
                const f32 sx = px, sy = py;
                constexpr int kSteps = 10;
                for (int k = 1; k <= kSteps; ++k) {
                    const f32 t = static_cast<f32>(k) / kSteps, u = 1.0f - t;
                    line(u * u * u * sx + 3 * u * u * t * c0x + 3 * u * t * t * c1x + t * t * t * x,
                         u * u * u * sy + 3 * u * u * t * c0y + 3 * u * t * t * c1y + t * t * t * y);
                }
            }
        }
    }

    Glyph bake(FontFile& f, int glyphIndex) {
        Glyph g;
        g.valid = true;
        const f32 em = 1.0f / kBakePixels;
        int advance = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&f.info, glyphIndex, &advance, &lsb);
        g.advance = static_cast<f32>(advance) * f.scale * em;
        stbtt_vertex* verts = nullptr;
        const int count = stbtt_GetGlyphShape(&f.info, glyphIndex, &verts);
        if (count <= 0) return g; // empty glyph (space)
        std::vector<f32> seg;
        flatten(verts, count, f.scale, seg);
        stbtt_FreeShape(&f.info, verts);
        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetGlyphBitmapBox(&f.info, glyphIndex, f.scale, f.scale, &ix0, &iy0, &ix1, &iy1);
        if (ix1 <= ix0 || iy1 <= iy0) return g;
        const int xoff = ix0 - kSdfPadding, yoff = iy0 - kSdfPadding;
        const int w = ix1 - ix0 + kSdfPadding * 2, h = iy1 - iy0 + kSdfPadding * 2;
        std::vector<u8> sdf(static_cast<size_t>(w) * h);
        const f32 maxDist = static_cast<f32>(kSdfPadding) + 1.0f;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const f32 px = static_cast<f32>(x + xoff) + 0.5f, py = static_cast<f32>(y + yoff) + 0.5f;
                f32 best = maxDist * maxDist;
                int winding = 0;
                for (size_t i = 0; i + 3 < seg.size(); i += 4) {
                    const f32 ax = seg[i], ay = seg[i + 1], bx = seg[i + 2], by = seg[i + 3];
                    // Nearest distance
                    const f32 dx = bx - ax, dy = by - ay;
                    const f32 len2 = dx * dx + dy * dy;
                    f32 t = len2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
                    t = std::clamp(t, 0.0f, 1.0f);
                    const f32 qx = ax + dx * t - px, qy = ay + dy * t - py;
                    best = std::min(best, qx * qx + qy * qy);
                    // Winding number (nonzero inside)
                    if (ay <= py) {
                        if (by > py && (dx * (py - ay) - (px - ax) * dy) > 0) ++winding;
                    } else if (by <= py && (dx * (py - ay) - (px - ax) * dy) < 0) {
                        --winding;
                    }
                }
                const f32 dist = std::sqrt(best) * (winding != 0 ? 1.0f : -1.0f);
                sdf[static_cast<size_t>(y) * w + x] = static_cast<u8>(std::clamp(kOnEdge + dist * kDistScale, 0.0f, 255.0f));
            }
        u32 ax = 0, ay = 0;
        if (!glyphShelves.place(static_cast<u32>(w), static_cast<u32>(h), ax, ay)) {
            MEALKIT_WARN("UI: glyph atlas full (%zu glyphs) - rebaking", glyphs.size());
            resetGlyphs();
            glyphShelves.place(static_cast<u32>(w), static_cast<u32>(h), ax, ay);
        }
        if (renderer) renderer->updateUiAtlas(render::UiAtlas::Glyphs, ax, ay, static_cast<u32>(w), static_cast<u32>(h), sdf.data());
        g.x0 = static_cast<f32>(xoff) * em;
        g.y0 = static_cast<f32>(yoff) * em;
        g.x1 = static_cast<f32>(xoff + w) * em;
        g.y1 = static_cast<f32>(yoff + h) * em;
        g.u0 = static_cast<f32>(ax) / kSize;
        g.v0 = static_cast<f32>(ay) / kSize;
        g.u1 = static_cast<f32>(ax + static_cast<u32>(w)) / kSize;
        g.v1 = static_cast<f32>(ay + static_cast<u32>(h)) / kSize;
        return g;
    }
};

Atlas::Atlas() : m(new Impl()) {}
Atlas::~Atlas() { delete m; }

void Atlas::setRenderer(render::Renderer* renderer) {
    m->renderer = renderer;
    m->resetGlyphs(); // a new renderer's atlas is empty
    m->imageShelves.clear();
    m->images.clear();
}

i32 Atlas::font(const std::string& name) {
    if (!m->fontsScanned) m->scanFonts();
    if (m->fonts.empty()) return -1;
    for (size_t i = 0; i < m->fonts.size(); ++i)
        if (m->fonts[i].name == name) return static_cast<i32>(i);
    return 0;
}

const FontMetrics& Atlas::metrics(i32 font) {
    if (font < 0 || font >= static_cast<i32>(m->fonts.size())) return m->defaultMetrics;
    return m->fonts[static_cast<size_t>(font)].metrics;
}

Glyph Atlas::glyph(i32& font, u32 codepoint) {
    if (font < 0 || font >= static_cast<i32>(m->fonts.size())) return m->none;
    i32 use = font;
    int index = stbtt_FindGlyphIndex(&m->fonts[static_cast<size_t>(use)].info, static_cast<int>(codepoint));
    for (size_t i = 0; index == 0 && i < m->fonts.size(); ++i) { // in other fonts
        const int other = stbtt_FindGlyphIndex(&m->fonts[i].info, static_cast<int>(codepoint));
        if (other) {
            use = static_cast<i32>(i);
            index = other;
        }
    }
    font = use;
    const u64 key = (static_cast<u64>(use) << 32) | codepoint;
    auto it = m->glyphs.find(key);
    if (it != m->glyphs.end()) return it->second;
    return m->glyphs.emplace(key, m->bake(m->fonts[static_cast<size_t>(use)], index)).first->second; // .notdef if missing
}

f32 Atlas::kerning(i32 font, u32 a, u32 b) {
    if (font < 0 || font >= static_cast<i32>(m->fonts.size())) return 0.0f;
    FontFile& f = m->fonts[static_cast<size_t>(font)];
    return static_cast<f32>(stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(a), static_cast<int>(b))) * f.scale /
           kBakePixels;
}

f32 Atlas::sdfUnitsPerPixel() const { return 255.0f / kDistScale; }

ImageInfo Atlas::image(const std::string& assetPath) {
    for (const Image& i : m->images)
        if (i.path == assetPath) return i.info;
    Image img;
    img.path = assetPath;
    const std::string file = findAssetFile(assetPath);
    img.time = file.empty() ? 0 : fileWriteTime(file);
    std::string bytes;
    int w = 0, h = 0, channels = 0;
    u8* pixels = nullptr;
    if (!file.empty() && readTextFile(file, bytes))
        pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &w, &h,
                                       &channels, 4);
    if (!pixels) {
        MEALKIT_WARN("UI: cannot load image assets/%s", assetPath.c_str());
    } else {
        u32 x = 0, y = 0;
        if (m->imageShelves.place(static_cast<u32>(w), static_cast<u32>(h), x, y)) {
            if (m->renderer)
                m->renderer->updateUiAtlas(render::UiAtlas::Images, x, y, static_cast<u32>(w), static_cast<u32>(h), pixels);
            img.info = {true, static_cast<u32>(w), static_cast<u32>(h), static_cast<f32>(x) / kSize, static_cast<f32>(y) / kSize,
                        static_cast<f32>(x + static_cast<u32>(w)) / kSize, static_cast<f32>(y + static_cast<u32>(h)) / kSize};
        } else {
            MEALKIT_WARN("UI: image atlas full - assets/%s is not shown", assetPath.c_str());
        }
        stbi_image_free(pixels);
    }
    m->images.push_back(std::move(img));
    return m->images.back().info;
}

void Atlas::reloadChanged() {
    bool fontsChanged = false;
    for (FontFile& f : m->fonts)
        if (fileWriteTime(f.path) != f.time) fontsChanged = true;
    if (fontsChanged) {
        MEALKIT_INFO("UI: fonts changed - reloading");
        m->scanFonts();
        m->resetGlyphs();
    }
    for (size_t i = 0; i < m->images.size(); ++i) {
        const std::string file = findAssetFile(m->images[i].path);
        if ((file.empty() ? 0 : fileWriteTime(file)) != m->images[i].time) {
            m->images.erase(m->images.begin() + static_cast<std::ptrdiff_t>(i)); // reread on next use (the atlas space is abandoned)
            --i;
        }
    }
}

u32 Atlas::glyphCount() const { return static_cast<u32>(m->glyphs.size()); }

} // namespace mealkit::ui
