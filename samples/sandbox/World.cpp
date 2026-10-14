#include "World.h"

#include "aster/core/Hash.h"
#include "aster/core/Profile.h"
#include "aster/core/Random.h"
#include "aster/render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

namespace sandbox {
namespace {

using aster::physics::BodyDesc;
using aster::physics::Motion;
using aster::scene::EntityKind;

constexpr Vec3 kGravity{0.0f, -20.0f, 0.0f};
constexpr f32 kPlayerSpeed = 7.0f;
constexpr f32 kJumpSpeed = 8.0f;
constexpr f32 kPushRadius = 7.0f;
constexpr u32 kPushCooldownTicks = 15;
constexpr f32 kBodyDensity = 200.0f; // kg/m³: 플레이어(80kg)가 밀 수 있을 만큼 가볍게

// 씬에 스폰 지점이 없을 때 쓰는 기본 위치 (삼각함수 대신 고정 테이블)
constexpr Vec3 kDefaultSpawns[aster::net::kMaxPlayers] = {
    {-6, kPlayerHalfHeight, -6}, {6, kPlayerHalfHeight, 6},  {6, kPlayerHalfHeight, -6}, {-6, kPlayerHalfHeight, 6},
    {0, kPlayerHalfHeight, -9},  {0, kPlayerHalfHeight, 9},  {-9, kPlayerHalfHeight, 0}, {9, kPlayerHalfHeight, 0},
};

constexpr u32 kPalette[] = {
    aster::render::rgba(232, 98, 82),  aster::render::rgba(245, 176, 65), aster::render::rgba(92, 172, 238),
    aster::render::rgba(120, 200, 120), aster::render::rgba(190, 120, 220), aster::render::rgba(236, 236, 230),
};

} // namespace

void World::reset(u64 seed, u32 playerCount, u32 bodyCount, const aster::scene::Scene& scene,
                  aster::asset::ModelCache& models, u32 physicsThreads) {
    m_physics.init(16384, physicsThreads);
    m_physics.setGravity(kGravity);
    m_bodyIds.clear();
    m_spawns.clear();

    // 1. 씬의 충돌 상자 (정적)
    for (const aster::scene::Entity& e : scene.entities) {
        if (e.kind == EntityKind::Solid) {
            BodyDesc d;
            d.motion = Motion::Static;
            d.position = e.position;
            d.rotation = e.rotation;
            d.friction = 0.8f;
            const aster::asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
            if (model) {
                std::vector<Vec3> points;
                points.reserve(model->hullPoints.size());
                for (const Vec3& p : model->hullPoints) points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
                m_physics.createConvexHull(points.data(), static_cast<u32>(points.size()), d);
            } else {
                m_physics.createBox(e.scale * 0.5f, d);
            }
        } else if (e.kind == EntityKind::SpawnPoint) {
            m_spawns.push_back(e.position);
        }
    }
    m_physics.optimize();

    // 2. 플레이어: 회전이 잠긴 캡슐. 속도를 직접 정해 움직이고, 물리적으로 상자를 밀어낸다.
    m_state = WorldState{};
    m_state.playerCount = playerCount;
    for (u32 i = 0; i < playerCount; ++i) {
        BodyDesc d;
        d.position = spawnPosition(i);
        d.lockRotation = true;
        d.friction = 0.0f;
        d.restitution = 0.0f;
        d.linearDamping = 0.0f;
        d.mass = 80.0f;
        d.allowSleep = false;
        m_playerIds[i] = m_physics.createCapsule(kPlayerHalfHeight - kPlayerRadius, kPlayerRadius, d);
    }

    // 3. 흩어진 상자들
    aster::Pcg32 rng(seed);
    m_state.bodies.resize(bodyCount);
    m_bodyIds.reserve(bodyCount);
    for (Body& b : m_state.bodies) {
        const f32 base = rng.range(0.28f, 0.64f);
        b.halfExtents = {base * rng.range(0.8f, 1.2f), base * rng.range(0.8f, 1.2f), base * rng.range(0.8f, 1.2f)};
        b.position = {rng.range(-40.0f, 40.0f), rng.range(1.0f, 25.0f), rng.range(-40.0f, 40.0f)};
        b.color = kPalette[rng.below(static_cast<u32>(std::size(kPalette)))];

        BodyDesc d;
        d.position = b.position;
        d.friction = 0.6f;
        d.restitution = 0.15f;
        d.mass = std::max(5.0f, kBodyDensity * 8.0f * b.halfExtents.x * b.halfExtents.y * b.halfExtents.z);
        m_bodyIds.push_back(m_physics.createBox(b.halfExtents, d));
    }
    syncState();
}

Vec3 World::spawnPosition(u32 player) const {
    if (m_spawns.empty()) return kDefaultSpawns[player % aster::net::kMaxPlayers];
    const u32 n = static_cast<u32>(m_spawns.size());
    const f32 offset = static_cast<f32>(player / n) * 1.2f; // 스폰 지점보다 사람이 많으면 옆으로 비켜 선다
    return m_spawns[player % n] + Vec3{offset, 0.0f, 0.0f};
}

void World::step(const aster::net::TickInputs& inputs) {
    ASTER_ZONE("World::step");

    // 입력 → 플레이어 속도 (플레이어 번호 순서대로)
    for (u32 i = 0; i < m_state.playerCount; ++i) {
        GameInput in;
        std::memcpy(&in, inputs.players[i].bytes, sizeof(in));
        Player& p = m_state.players[i];
        const aster::physics::BodyId id = m_playerIds[i];

        Vec3 wish{static_cast<f32>(in.moveX) / 127.0f, 0.0f, static_cast<f32>(in.moveZ) / 127.0f};
        const f32 wishLen2 = aster::lengthSq(wish);
        if (wishLen2 > 1.0f) wish = wish / std::sqrt(wishLen2);

        Vec3 v = m_physics.linearVelocity(id);
        const f32 accel = std::min(1.0f, (p.grounded ? 12.0f : 3.0f) * kTickDt);
        v.x += (wish.x * kPlayerSpeed - v.x) * accel;
        v.z += (wish.z * kPlayerSpeed - v.z) * accel;
        if (p.grounded && (in.buttons & kButtonJump) && v.y < 1.0f) v.y = kJumpSpeed;
        m_physics.setLinearVelocity(id, v);

        if (p.pushCooldown > 0) {
            --p.pushCooldown;
        } else if (in.buttons & kButtonPush) {
            p.pushCooldown = kPushCooldownTicks;
            push(p.position);
        }
    }

    m_physics.step(kTickDt, 2); // 30Hz 틱을 물리 2단계로 나눠 안정성을 높인다

    // 떨어진 것 되돌리기 (순서 고정)
    for (u32 i = 0; i < m_state.playerCount; ++i) {
        if (m_physics.position(m_playerIds[i]).y < kKillPlaneY) {
            m_physics.setTransform(m_playerIds[i], spawnPosition(i), {});
            m_physics.setLinearVelocity(m_playerIds[i], {});
        }
    }
    for (aster::physics::BodyId id : m_bodyIds) {
        const Vec3 p = m_physics.position(id);
        if (p.y < kKillPlaneY) {
            m_physics.setTransform(id, {std::clamp(p.x, -40.0f, 40.0f), 30.0f, std::clamp(p.z, -40.0f, 40.0f)}, {});
            m_physics.setLinearVelocity(id, {});
            m_physics.setAngularVelocity(id, {});
        }
    }

    syncState();
    ++m_state.tick;
}

void World::push(const Vec3& center) {
    for (size_t i = 0; i < m_bodyIds.size(); ++i) {
        const Vec3 d = m_state.bodies[i].position - center;
        const f32 dist2 = aster::lengthSq(d);
        if (dist2 >= kPushRadius * kPushRadius) continue;
        const f32 dist = std::sqrt(dist2);
        const Vec3 dir = dist > 1e-4f ? d / dist : Vec3{0, 1, 0};
        const f32 strength = 1.0f - dist / kPushRadius;
        m_physics.addLinearVelocity(m_bodyIds[i], dir * (18.0f * strength) + Vec3{0.0f, 7.0f * strength, 0.0f});
        m_physics.setAngularVelocity(m_bodyIds[i], m_physics.angularVelocity(m_bodyIds[i]) + Vec3{dir.z, 0.0f, -dir.x} * (8.0f * strength));
    }
}

void World::syncState() {
    for (u32 i = 0; i < m_state.playerCount; ++i) {
        Player& p = m_state.players[i];
        const aster::physics::BodyId id = m_playerIds[i];
        p.position = m_physics.position(id);
        p.velocity = m_physics.linearVelocity(id);
        aster::physics::RayHit hit;
        p.grounded = m_physics.castRay(p.position, {0.0f, -(kPlayerHalfHeight + 0.12f), 0.0f}, hit, id) ? 1u : 0u;
    }
    for (size_t i = 0; i < m_bodyIds.size(); ++i) {
        Body& b = m_state.bodies[i];
        b.position = m_physics.position(m_bodyIds[i]);
        b.rotation = m_physics.rotation(m_bodyIds[i]);
    }
}

u64 World::hash() const {
    aster::Hasher h;
    h.add(m_state.tick);
    for (u32 i = 0; i < m_state.playerCount; ++i) {
        h.add(m_state.players[i].pushCooldown);
        h.add(m_state.players[i].grounded);
    }
    m_physics.hashState(h);
    return h.value;
}

} // namespace sandbox
