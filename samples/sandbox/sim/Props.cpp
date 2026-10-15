// 흩어진 상자: 무작위 생성(Pcg32), 밀치기, 떨어지면 위에서 다시 떨어뜨리기.
#include "sim/Systems.h"

#include "mealkit/core/Random.h"

#include <algorithm>
#include <cmath>

namespace sandbox {
namespace {

constexpr f32 kPropDensity = 200.0f; // kg/m³: 플레이어(80kg)가 밀 수 있을 만큼 가볍게

} // namespace

void spawnProps(World& world, u64 seed, u32 count) {
    mealkit::Pcg32 rng(seed);
    for (u32 i = 0; i < count; ++i) {
        Prop b;
        const f32 base = rng.range(0.28f, 0.64f);
        b.halfExtents = {base * rng.range(0.8f, 1.2f), base * rng.range(0.8f, 1.2f), base * rng.range(0.8f, 1.2f)};
        b.position = {rng.range(-40.0f, 40.0f), rng.range(1.0f, 25.0f), rng.range(-40.0f, 40.0f)};
        b.colorIndex = rng.below(kPropColorCount);

        mealkit::physics::BodyDesc d;
        d.position = b.position;
        d.friction = 0.6f;
        d.restitution = 0.15f;
        d.mass = std::max(5.0f, kPropDensity * 8.0f * b.halfExtents.x * b.halfExtents.y * b.halfExtents.z);
        b.body = world.physics.createBox(b.halfExtents, d);
        world.state.props.create(b);
    }
}

void pushProps(World& world, const Vec3& center) {
    for (auto [id, prop] : world.state.props) {
        const Vec3 d = prop.position - center;
        const f32 dist2 = mealkit::lengthSq(d);
        if (dist2 >= kPushRadius * kPushRadius) continue;
        const f32 dist = std::sqrt(dist2);
        const Vec3 dir = dist > 1e-4f ? d / dist : Vec3{0, 1, 0};
        const f32 strength = 1.0f - dist / kPushRadius;
        world.physics.addLinearVelocity(prop.body, dir * (18.0f * strength) + Vec3{0.0f, 7.0f * strength, 0.0f});
        world.physics.setAngularVelocity(prop.body,
                                         world.physics.angularVelocity(prop.body) + Vec3{dir.z, 0.0f, -dir.x} * (8.0f * strength));
    }
}

void recoverFallenProps(World& world) {
    for (auto [propId, prop] : world.state.props) {
        const mealkit::physics::BodyId id = prop.body;
        const Vec3 p = world.physics.position(id);
        if (p.y < kKillPlaneY) {
            world.physics.setTransform(id, {std::clamp(p.x, -40.0f, 40.0f), 30.0f, std::clamp(p.z, -40.0f, 40.0f)}, {});
            world.physics.setLinearVelocity(id, {});
            world.physics.setAngularVelocity(id, {});
        }
    }
}

void senseProps(World& world) {
    for (auto [id, prop] : world.state.props) {
        prop.position = world.physics.position(prop.body);
        prop.rotation = world.physics.rotation(prop.body);
    }
}

} // namespace sandbox
