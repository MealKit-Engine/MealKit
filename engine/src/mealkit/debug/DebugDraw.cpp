#include "mealkit/debug/DebugDraw.h"
#include "mealkit/debug/DebugDrawRender.h"

#include <imgui.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace mealkit::debug {
namespace {

struct Item {
    enum class Kind : u8 { Line, Text } kind;
    Vec3 a, b;
    u32 color;
    f32 seconds;
    f64 expires = -1.0; // set on first draw
    std::string text;
};

std::mutex g_lock; // can be called from the simulation worker thread
std::vector<Item> g_items;
constexpr size_t kMaxItems = 20000; // guards against the mistake of piling up without clearing

void push(Item item) {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_items.size() < kMaxItems) g_items.push_back(std::move(item));
}

// Rotation that turns the z axis to dir
Quat lookRotation(Vec3 dir) {
    const Vec3 z{0.0f, 0.0f, 1.0f};
    const f32 d = dot(z, dir);
    if (d < -0.9999f) return {0.0f, 1.0f, 0.0f, 0.0f};
    const Vec3 c = cross(z, dir);
    return normalize(Quat{c.x, c.y, c.z, 1.0f + d});
}

} // namespace

void line(Vec3 a, Vec3 b, u32 color, f32 seconds) { push({Item::Kind::Line, a, b, color, seconds}); }

void box(Vec3 center, Vec3 half, Quat rotation, u32 color, f32 seconds) {
    Vec3 corners[8];
    for (int i = 0; i < 8; ++i)
        corners[i] = center + rotate(rotation, {i & 1 ? half.x : -half.x, i & 2 ? half.y : -half.y, i & 4 ? half.z : -half.z});
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& e : edges) line(corners[e[0]], corners[e[1]], color, seconds);
}

void text(Vec3 position, const char* text, u32 color, f32 seconds) {
    push({Item::Kind::Text, position, {}, color, seconds, -1.0, text ? text : ""});
}

void flushDebugDraw(render::DrawList& out, render::MeshId boxMesh, const render::FrameParams& frame, f64 now, bool drawText) {
    std::lock_guard<std::mutex> guard(g_lock);
    const Mat4& vp = frame.viewProj;
    for (Item& it : g_items) {
        if (it.expires < 0.0) it.expires = now + it.seconds;
        if (it.kind == Item::Kind::Line) {
            const Vec3 d = it.b - it.a;
            const f32 len = length(d);
            if (len < 1e-5f) continue;
            render::InstanceData inst;
            inst.position = (it.a + it.b) * 0.5f;
            inst.rotation = lookRotation(d * (1.0f / len));
            inst.scale = {0.03f, 0.03f, len};
            inst.color = it.color;
            out.add(boxMesh, inst);
        } else if (drawText) {
            const Vec3 p = it.a;
            const f32 cx = p.x * vp.m[0][0] + p.y * vp.m[1][0] + p.z * vp.m[2][0] + vp.m[3][0];
            const f32 cy = p.x * vp.m[0][1] + p.y * vp.m[1][1] + p.z * vp.m[2][1] + vp.m[3][1];
            const f32 cw = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
            if (cw <= 0.0f) continue;
            const render::Viewport& v = frame.viewport;
            const ImVec2 at(v.x + (cx / cw * 0.5f + 0.5f) * v.width, v.y + (0.5f - cy / cw * 0.5f) * v.height);
            ImDrawList* draw = ImGui::GetForegroundDrawList();
            draw->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 200), it.text.c_str());
            draw->AddText(at, it.color, it.text.c_str());
        }
    }
    // Removes expired items among those already drawn once (seconds = 0 means this one time)
    std::erase_if(g_items, [&](const Item& it) { return it.expires <= now; });
}

} // namespace mealkit::debug
