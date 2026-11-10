#include "view/LocalInput.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

GameInput LocalInput::sample(const input::Actions& actions, const GameCamera& camera) const {
    GameInput g;
    (void)camera; // 2D: screen right is world +x whatever the camera does
    g.moveX = quantize(actions.axis2("move").x);
    g.moveZ = 0;
    if (jumpLatch || actions.down("jump")) g.buttons |= kButtonJump;
    return g;
}

} // namespace sandbox
