#include "view/Prediction.h"

#include "sim/Systems.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace mealkit;

namespace sandbox {

void LocalPrediction::addInput(const World& world, u8 player, const net::PlayerInput& input) {
    GameInput g;
    std::memcpy(&g, input.bytes, sizeof(g));
    const BodyKind* body = bodyOf(world, player);
    const f32 speed = world.tuning.playerSpeed * (body ? body->speedScale : 1.0f); // 시뮬레이션의 movePawn과 같은 속도
    Vec3 wish{static_cast<f32>(g.moveX) / 127.0f, 0.0f, static_cast<f32>(g.moveZ) / 127.0f};
    const f32 len2 = lengthSq(wish);
    if (len2 > 1.0f) wish = wish / std::sqrt(len2);
    m_target += wish * (speed * kTickDt); // 한 틱 동안 갈 거리
}

void LocalPrediction::update(f32 dt, const PredictionLook& look, bool active) {
    Vec3 target = active && look.enabled ? m_target : Vec3{};
    const f32 len = length(target);
    if (len > look.maxOffset) target = target * (look.maxOffset / len);
    offset += (target - offset) * std::min(1.0f, dt * look.follow);
}

} // namespace sandbox
