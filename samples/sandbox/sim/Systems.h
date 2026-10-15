#pragma once

#include "sim/World.h"

// sim/ 시스템 함수. World::step()이 정해진 순서로 직접 부른다 (World.cpp). 각자 Update를 도는 방식은 쓰지 않는다.
// 시스템끼리 알릴 일이 있으면 함수를 직접 호출한다 (예: controlPawns → pushProps).
namespace sandbox {

// Pawns.cpp
Vec3 spawnPosition(const World& world, u32 index);
void createControllers(World& world);     // 매치 시작: 플레이어 슬롯마다 하나, 그다음 AI
Id<Pawn> spawnPawn(World& world, const Vec3& position);
void possess(World& world, Id<Controller> controller, Id<Pawn> pawn);
void unpossess(World& world, Id<Controller> controller);
void respawnPawns(World& world);          // Pawn이 없고 리스폰 대기가 끝난 컨트롤러에 새 Pawn
void controlPawns(World& world);          // 입력·AI → 속도, 점프, 밀치기
void usePads(World& world);               // 점프대 위의 Pawn을 튕겨 올린다
void removeFallenPawns(World& world);     // 떨어진 Pawn을 없애고 리스폰을 기다린다
void sensePawns(World& world);            // 물리 결과 → 위치, 속도, 접지

// Props.cpp
void spawnProps(World& world, u64 seed, u32 count);
void pushProps(World& world, const Vec3& center);
void recoverFallenProps(World& world);
void senseProps(World& world); // 물리 결과 → 위치, 회전

} // namespace sandbox
