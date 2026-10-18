// 흩어진 상자: 무작위 생성(Pcg32), 밀치기, 떨어지면 위에서 다시 떨어뜨리기.
#include "sim/Systems.h"

#include "mealkit/core/Random.h"

#include <algorithm>
#include <cmath>

namespace sandbox {

void spawnProps(World& world, u64 seed) {
    const Tuning& t = world.tuning;
    mealkit::Pcg32 rng(seed);
    for (const World::PropSpawn& area : world.propSpawns) {
        for (u32 i = 0; i < area.count; ++i) {
            Prop b;
            const f32 base = rng.range(t.propSizeMin, t.propSizeMax);
            b.halfExtents = {base * rng.range(t.propStretchMin, t.propStretchMax),
                             base * rng.range(t.propStretchMin, t.propStretchMax),
                             base * rng.range(t.propStretchMin, t.propStretchMax)};
            b.position = {rng.range(area.min.x, area.max.x), rng.range(area.min.y, area.max.y),
                          rng.range(area.min.z, area.max.z)};
            b.colorIndex = rng.below(kPropColorCount);

            mealkit::physics::BodyDesc d;
            d.position = b.position;
            d.friction = t.propFriction;
            d.restitution = t.propRestitution;
            const Vec3 h = b.halfExtents;
            d.mass = std::max(t.propMinMass, t.propDensity * 8.0f * h.x * h.y * h.z); // const-ok: 부피 = 8 × 반 크기 곱
            b.body = world.physics.createBox(b.halfExtents, d);
            world.state.props.create(b);
        }
    }
}

void pushProps(World& world, const Vec3& center) {
    const Tuning& t = world.tuning;
    for (auto [id, prop] : world.state.props) {
        const Vec3 d = prop.position - center;
        const f32 dist2 = mealkit::lengthSq(d);
        if (dist2 >= t.pushRadius * t.pushRadius) continue;
        const f32 dist = std::sqrt(dist2);
        const Vec3 dir = dist > 1e-4f ? d / dist : Vec3{0, 1, 0}; // const-ok: 0으로 나누지 않는 문턱
        const f32 strength = 1.0f - dist / t.pushRadius;
        world.physics.addLinearVelocity(prop.body,
                                        dir * (t.pushSpeed * strength) + Vec3{0.0f, t.pushLift * strength, 0.0f});
        world.physics.setAngularVelocity(prop.body, world.physics.angularVelocity(prop.body) +
                                                        Vec3{dir.z, 0.0f, -dir.x} * (t.pushSpin * strength));
    }
}

// 경기 영역 바닥 아래로 떨어진 상자는 영역 위에서 다시 떨어뜨린다
void recoverFallenProps(World& world) {
    const Vec3 lo = world.arenaMin, hi = world.arenaMax;
    for (auto [propId, prop] : world.state.props) {
        const mealkit::physics::BodyId id = prop.body;
        const Vec3 p = world.physics.position(id);
        if (p.y < lo.y) {
            world.physics.setTransform(id, {std::clamp(p.x, lo.x, hi.x), hi.y, std::clamp(p.z, lo.z, hi.z)}, {});
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
