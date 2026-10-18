#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"

// 정의 데이터 표 (docs/CODING_MODEL.md "같은 모양의 데이터가 여러 개"): 같은 모양의 행이 여러 개인 게임 데이터.
// 값은 assets/tables/*.table (에디터 [표] 패널, CSV 가져오기·내보내기). 매치를 시작할 때 app/Headless.cpp가 읽어 World에 복사한다.
// 다른 데이터(씬 물체 등)는 행을 RowRef<행 타입>(이름 id)으로 가리킨다.
namespace sandbox {

// 상자 종류: 상자 생성 영역(PropSpawnArea.kind)이 고른다
MEALKIT_STRUCT(PropKind) {
    MEALKIT_FIELD(mealkit::f32, sizeMin, 0.28f, "반 크기 최소 (m)");
    MEALKIT_FIELD(mealkit::f32, sizeMax, 0.64f, "반 크기 최대 (m)");
    MEALKIT_FIELD(mealkit::f32, stretchMin, 0.8f, "축별 늘림 최소 (배)");
    MEALKIT_FIELD(mealkit::f32, stretchMax, 1.2f, "축별 늘림 최대 (배)");
    MEALKIT_FIELD(mealkit::f32, density, 200.0f, "밀도 (kg/m³)");
    MEALKIT_FIELD(mealkit::f32, minMass, 5.0f, "최소 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, friction, 0.6f, "마찰");
    MEALKIT_FIELD(mealkit::f32, restitution, 0.15f, "반발");
};
MEALKIT_TABLE(PropKind, "tables/props.table", 0); // 시뮬레이션 표: 콘텐츠 해시에 들어간다

// 캐릭터 몸 종류: 플레이어마다 매치 시작 데이터(로드아웃)로 고른다. AI와 고르지 않은 플레이어는 첫 행
MEALKIT_STRUCT(BodyKind) {
    MEALKIT_FIELD(mealkit::f32, speedScale, 1.0f, "이동 속도 배율");
    MEALKIT_FIELD(mealkit::f32, massScale, 1.0f, "질량 배율");
};
MEALKIT_TABLE(BodyKind, "tables/bodies.table", 0);

} // namespace sandbox
