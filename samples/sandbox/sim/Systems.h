#pragma once

#include "sim/World.h"

// sim/ 시스템 함수. World::step()이 정해진 순서로 직접 부른다 (World.cpp). 각자 Update를 도는 방식은 쓰지 않는다.
// 시스템끼리 알릴 일이 있으면 함수를 직접 호출한다 (예: movePlayers → pushProps).
namespace sandbox {

// Players.cpp
Vec3 spawnPosition(const World& world, u32 player);
void spawnPlayers(World& world);
void movePlayers(World& world); // 입력 → 속도, 점프, 밀치기
void respawnFallenPlayers(World& world);
void usePads(World& world); // 점프대 위의 플레이어를 튕겨 올린다
void sensePlayers(World& world); // 물리 결과 → 위치, 속도, 접지

// Props.cpp
void spawnProps(World& world, u64 seed, u32 count);
void pushProps(World& world, const Vec3& center);
void recoverFallenProps(World& world);
void senseProps(World& world); // 물리 결과 → 위치, 회전

} // namespace sandbox
