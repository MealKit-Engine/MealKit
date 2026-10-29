#include "view/LocalInput.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

void LocalInput::poll(const input::Actions& actions) {
    pushLatch |= actions.pressed("push");
    jumpLatch |= actions.pressed("jump");
}

GameInput LocalInput::sample(const input::Actions& actions, const OrbitCamera& camera) const {
    GameInput g;
    const Vec3 forward = camera.forward();
    const Vec3 right = cross(Vec3{0, 1, 0}, forward);
    const Vec2 stick = actions.axis2("move"); // length at most 1 (1 for keys, as far as tilted for sticks)
    const f32 amount = std::min(length(stick), 1.0f);
    const Vec3 move = normalize(forward * stick.y + right * stick.x) * amount;
    g.moveX = quantize(move.x);
    g.moveZ = quantize(move.z);
    if (jumpLatch || actions.down("jump")) g.buttons |= kButtonJump;
    if (pushLatch) g.buttons |= kButtonPush;
    return g;
}

} // namespace sandbox
