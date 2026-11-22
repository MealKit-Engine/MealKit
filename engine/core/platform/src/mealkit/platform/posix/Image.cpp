// Images on Linux/macOS: stb_image (PNG, JPEG; its implementation is ui/StbImpl.cpp) and stb_image_write (PNG, static here).
#include "mealkit/platform/Image.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace mealkit {
namespace {

// Box filter scale-down so the long side is at most maxSize
void shrink(Image& image, u32 maxSize) {
    const u32 w = image.width, h = image.height;
    if (!maxSize || (w <= maxSize && h <= maxSize)) return;
    const double s = static_cast<double>(maxSize) / std::max(w, h);
    const u32 nw = std::max(1u, static_cast<u32>(w * s + 0.5)), nh = std::max(1u, static_cast<u32>(h * s + 0.5));
    std::vector<u8> out(static_cast<size_t>(nw) * nh * 4);
    for (u32 y = 0; y < nh; ++y) {
        const u32 y0 = y * h / nh, y1 = std::max(y0 + 1, (y + 1) * h / nh);
        for (u32 x = 0; x < nw; ++x) {
            const u32 x0 = x * w / nw, x1 = std::max(x0 + 1, (x + 1) * w / nw);
            u32 sum[4] = {};
            for (u32 sy = y0; sy < y1; ++sy)
                for (u32 sx = x0; sx < x1; ++sx)
                    for (int c = 0; c < 4; ++c) sum[c] += image.rgba[(static_cast<size_t>(sy) * w + sx) * 4 + c];
            const u32 n = (y1 - y0) * (x1 - x0);
            for (int c = 0; c < 4; ++c) out[(static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<u8>(sum[c] / n);
        }
    }
    image = {nw, nh, std::move(out)};
}

} // namespace

bool loadImage(const std::string& path, u32 maxSize, Image& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!pixels) return false;
    out.width = static_cast<u32>(w);
    out.height = static_cast<u32>(h);
    out.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
    stbi_image_free(pixels);
    shrink(out, maxSize);
    return true;
}

// Executables have no resource section here; the editor falls back to its built-in look
bool loadImageResource(int, u32, Image&) { return false; }

bool savePng(const std::string& path, const Image& image) {
    if (image.rgba.empty()) return false;
    return stbi_write_png(path.c_str(), static_cast<int>(image.width), static_cast<int>(image.height), 4,
                          image.rgba.data(), static_cast<int>(image.width * 4)) != 0;
}

} // namespace mealkit
