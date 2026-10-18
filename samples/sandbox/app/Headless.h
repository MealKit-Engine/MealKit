#pragma once

#include "app/ProjectSettings.h"
#include "sim/World.h"

#include "mealkit/game/Headless.h"

// 헤드리스 진입점 (mealkit/game/Headless.h): 창·렌더러 없이 World만 돌릴 때 엔진이 부르는 함수들.
// sandbox_headless 실행 파일은 sim/ + app/Headless.cpp만으로 빌드된다 → 여기서는 view/와 렌더러 헤더를 쓰지 않는다.
// GameSession(창 있는 게임, 에디터)도 매치 시작과 봇 입력에 같은 함수를 쓴다.
namespace sandbox {

struct Headless {
    using World = sandbox::World;
    using TuningType = Tuning;
    using ProjectSettingsType = ProjectSettings;
    static constexpr mealkit::f32 kTickDt = sandbox::kTickDt;

    // 매치 시작: 상자 수는 씬의 상자 생성 영역(matchData가 0이 아니면 그 수). 상자는 매치 중에 늘지 않으므로 Pool 용량 = 상자 수
    static void startWorld(World& world, const mealkit::game::MatchInfo& match, const Tuning& tuning,
                           const mealkit::scene::Scene& flatScene, mealkit::asset::ModelCache& models,
                           u32 physicsThreads);
    // 스크립트 입력(--bot): 원을 그리며 걷고 주기적으로 밀치기/점프
    static mealkit::net::PlayerInput botInput(u32 tick, u8 player);
    // 봇의 확정 명령: 가끔 디버그 충격파 (명령 채널을 결정론 시험에 넣는다)
    static void botCommands(u32 tick, u8 player, mealkit::net::LockstepSession& session);
    // 봇의 매치 시작 데이터: 플레이어마다 다른 몸 (로드아웃 경로를 결정론 시험에 넣는다)
    static void botLoadout(u8 player, mealkit::net::LockstepSession& session);
    // 프로젝트 설정의 기본 포트·인원
    static void defaults(mealkit::game::HeadlessOptions& options, const ProjectSettings& project);
};

} // namespace sandbox
