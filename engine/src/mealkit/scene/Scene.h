#pragma once

#include "mealkit/core/Math.h"

#include <string>
#include <vector>

// 씬: 에디터에서 배치하고 게임이 불러 쓰는 물체 목록과 환경 설정.
// 파일은 사람이 읽을 수 있는 텍스트(.scene)다. 실수는 %.9g로 저장해 읽고 쓸 때 비트 단위로 보존된다.
// 멀티플레이에서는 모든 피어가 같은 씬 파일을 써야 하므로 contentHash()로 접속 시 검사한다.
namespace mealkit::scene {

enum class EntityKind : u8 {
    Solid,      // 보이고 충돌하는 상자
    Decoration, // 보이기만 하는 상자
    SpawnPoint, // 플레이어 시작 위치 (게임에서는 보이지 않음)
};

const char* kindName(EntityKind kind); // 파일용 영문 이름

struct Entity {
    u32 id = 0;
    std::string name;
    EntityKind kind = EntityKind::Solid;
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    u32 color = 0xFFC8C8C8; // rgba8 sRGB (render::rgba와 같은 배치). 모델의 재질 색과 곱해진다
    u32 flags = 0;          // render::kInstance* 플래그
    std::string model;      // glTF 경로 (assets 폴더 기준, 예: models/tree.glb). 비어 있으면 상자
};

struct Environment {
    Vec3 sunDirection{0.4f, 0.8f, 0.3f};
    Vec3 sunColor{1.0f, 0.95f, 0.85f};
    Vec3 skyColor{0.30f, 0.38f, 0.50f};
    Vec3 groundColor{0.20f, 0.17f, 0.14f};
    Vec3 fogColor{0.50f, 0.62f, 0.78f};
    f32 fogDensity = 0.008f;
};

struct Scene {
    std::vector<Entity> entities;
    Environment environment;
    u32 nextId = 1;

    Entity& add(EntityKind kind, const std::string& name);
    Entity* find(u32 id);
    const Entity* find(u32 id) const;
    int indexOf(u32 id) const; // 없으면 -1

    // 게임 결과에 영향을 주는 데이터(충돌체, 스폰 지점)의 해시. 색이나 장식은 포함하지 않는다.
    u64 contentHash() const;
};

bool save(const Scene& scene, const std::string& path);
bool load(Scene& scene, const std::string& path, std::string* error = nullptr);

// 기본 아레나: 바닥, 벽, 경사로, 기둥, 스폰 지점 4개. 회전은 삼각함수 없이 상수로 넣어 어느 PC에서나 같다.
Scene makeDefault();

} // namespace mealkit::scene
