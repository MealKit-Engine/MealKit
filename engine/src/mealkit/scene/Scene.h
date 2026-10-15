#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"

#include <string>
#include <vector>

// 씬: 에디터에서 배치하고 게임이 불러 쓰는 물체 목록과 환경 설정.
// 파일은 사람이 읽을 수 있는 텍스트(.scene)다. 실수는 %.9g로 저장해 읽고 쓸 때 비트 단위로 보존된다.
// 멀티플레이에서는 모든 피어가 같은 씬 파일을 써야 하므로 contentHash()로 접속 시 검사한다.
//
// 물체 종류(type): 엔진 기본 종류(solid, decoration, spawn)와 게임이 MEALKIT_OBJECT로 선언한 종류.
// 게임 종류의 필드 값은 텍스트로 들고 있어서, 게임 코드가 없거나 필드가 바뀌어도 씬 파일의 값이 사라지지 않는다.
// 게임은 매치를 시작할 때 readObject<T>(entity)로 읽는다.
namespace mealkit::scene {

inline constexpr const char* kTypeSolid = "solid";           // 보이고 충돌하는 상자·모델
inline constexpr const char* kTypeDecoration = "decoration"; // 보이기만 하는 상자·모델
inline constexpr const char* kTypeSpawn = "spawn";           // 플레이어 시작 위치 (게임에서는 보이지 않음)

// 엔진 기본 종류의 설명 (필드 없음, 라벨과 플래그만)
const std::vector<const reflect::TypeInfo*>& builtinTypes();

struct Entity {
    u32 id = 0;
    u32 parent = 0; // 부모 물체 id (0 = 최상위). 위치·회전·크기는 부모 기준이다
    std::string name;
    std::string type = kTypeSolid;
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
    u32 color = 0xFFC8C8C8; // rgba8 sRGB (render::rgba와 같은 배치). 모델의 재질 색과 곱해진다
    u32 flags = 0;          // render::kInstance* 플래그
    std::string model;      // glTF 경로 (assets 폴더 기준, 예: models/tree.glb). 비어 있으면 상자
    reflect::TextFields fields; // 게임 종류의 필드 (경로, 값 텍스트)

    bool is(const char* typeName) const { return type == typeName; }
    template <class T>
    bool is() const {
        return type == reflect::typeOf<T>().name;
    }
};

struct Environment {
    Vec3 sunDirection{0.4f, 0.8f, 0.3f};
    Vec3 sunColor{1.0f, 0.95f, 0.85f};
    Vec3 skyColor{0.30f, 0.38f, 0.50f};
    Vec3 groundColor{0.20f, 0.17f, 0.14f};
    Vec3 fogColor{0.50f, 0.62f, 0.78f};
    f32 fogDensity = 0.008f;
};

using TypeList = std::vector<const reflect::TypeInfo*>;

struct Scene {
    std::vector<Entity> entities;
    Environment environment;
    u32 nextId = 1;

    Entity& add(const std::string& type, const std::string& name);
    Entity* find(u32 id);
    const Entity* find(u32 id) const;
    int indexOf(u32 id) const; // 없으면 -1

    // 게임 결과에 영향을 주는 데이터의 해시: 장식 종류(kObjectCosmetic)와 보기 전용 필드(kFieldView), 색은 뺀다.
    // gameTypes: 게임이 선언한 종류 (없으면 게임 종류의 모든 필드를 넣는다).
    u64 contentHash(const TypeList* gameTypes = nullptr) const;
};

// ---------------------------------------------------------------- 계층 (부모·자식)
// 에디터에서만 트리로 다룬다. 게임은 불러올 때 flattened()로 월드 좌표로 펼쳐 평평한 배열로 쓴다 (속도·결정론 유지).
// 크기는 축별로 곱한다 (회전된 부모의 비균등 크기에서 생기는 찌그러짐은 버린다).

struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

Transform localTransform(const Entity& e);
Transform worldTransform(const Scene& scene, const Entity& e);
// 월드 변환을 주면 부모 기준 값으로 바꿔 넣는다
void setWorldTransform(const Scene& scene, Entity& e, const Transform& world);
// id가 ancestor의 자손인가 (자기 자신 포함)
bool isDescendant(const Scene& scene, u32 id, u32 ancestor);
// id와 그 자손들의 id (entities 순서)
std::vector<u32> subtree(const Scene& scene, u32 id);
// 모든 물체를 월드 좌표로 펼친 사본 (parent = 0, 순서 그대로). 게임이 매치를 시작할 때 쓴다.
Scene flattened(const Scene& scene);

// 종류 이름으로 설명을 찾는다 (기본 종류 → gameTypes 순서). 없으면 nullptr.
const reflect::TypeInfo* findType(const std::string& name, const TypeList* gameTypes);

// 게임 종류의 필드를 읽는다. 씬에 없는 필드는 선언의 기본값.
template <class T>
T readObject(const Entity& e) {
    T value{};
    std::string warnings;
    reflect::fromText(reflect::typeOf<T>(), &value, e.fields, &warnings);
    return value;
}

// 기본값으로 채운 필드 목록 (에디터가 새 물체를 만들 때)
reflect::TextFields defaultFields(const reflect::TypeInfo& type);

bool save(const Scene& scene, const std::string& path);
bool load(Scene& scene, const std::string& path, std::string* error = nullptr);

// 기본 아레나: 바닥, 벽, 경사로, 기둥, 스폰 지점 4개. 회전은 삼각함수 없이 상수로 넣어 어느 PC에서나 같다.
Scene makeDefault();

} // namespace mealkit::scene
