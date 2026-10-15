#include "view/LocalInput.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

void LocalInput::poll(Input& input, bool keyboardAllowed, bool mouseAllowed) {
    pushLatch |= mouseAllowed && input.mousePressed(MouseButton::Left);
    jumpLatch |= keyboardAllowed && input.keyPressed(key::Space);
}

GameInput LocalInput::sample(Input& input, const OrbitCamera& camera, bool keyboardAllowed, bool mouseAllowed) const {
    GameInput g;
    if (!keyboardAllowed && !mouseAllowed) return g;
    const Vec3 forward = camera.forward();
    const Vec3 right = cross(Vec3{0, 1, 0}, forward);
    Vec3 move{};
    if (keyboardAllowed) {
        if (input.keyDown('W')) move += forward;
        if (input.keyDown('S')) move -= forward;
        if (input.keyDown('D')) move += right;
        if (input.keyDown('A')) move -= right;
    }
    move = normalize(move);
    g.moveX = quantize(move.x);
    g.moveZ = quantize(move.z);
    if (jumpLatch || (keyboardAllowed && input.keyDown(key::Space))) g.buttons |= kButtonJump;
    if (pushLatch) g.buttons |= kButtonPush;
    return g;
}

GameInput botInput(u32 tick, u8 player) {
    GameInput g;
    const f32 a = static_cast<f32>(tick) * 0.05f + static_cast<f32>(player) * 1.7f;
    g.moveX = quantize(std::sin(a));
    g.moveZ = quantize(std::cos(a));
    if ((tick + player * 11u) % 40u == 0) g.buttons |= kButtonPush;
    if ((tick + player * 7u) % 75u == 0) g.buttons |= kButtonJump;
    return g;
}

} // namespace sandbox
