#include "app/GameSession.h"
#include "app/Headless.h"
#include "sim/Commands.h"

#include "mealkit/feedback/Feedback.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/Save.h"
#include "mealkit/sim/Commands.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/ui/Ui.h"
#include "mealkit/platform/Window.h"

#if MEALKIT_DEBUG_UI
#include <imgui.h>
#endif

#include <cstring>

using namespace mealkit;

namespace sandbox {

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    const net::LockstepSession& session = ctx.session;
    m_scene = scene::flattened(scene); // 부모·자식 계층을 월드 좌표로 펼친다 (시뮬레이션과 그리기는 평평한 목록만 본다)
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match{session.seed(), session.playerCount(), session.matchData()};
    m_sim.start(options, kTickDt, [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // 헤드리스와 같은 매치 시작
    });
    m_input.clearLatches();
    m_effects = {};
    ctx.audio.stop(m_music);
    m_music = ctx.audio.play2D("music");
    if (!m_saveLoaded) {
        game::readSave(m_save, ctx.userFolder, "progress", kSaveVersion);
        m_saveLoaded = true;
    }
    ++m_save.matchesStarted;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!ctx.ui.isOpen(m_hud)) m_hud = ctx.ui.open("ui/hud.ui");
    if (!ctx.ui.isOpen(m_nameTag)) m_nameTag = ctx.ui.openWorld("ui/nametag.ui");
    ctx.ui.bind(m_hud, m_hudVm);
}

void GameSession::setTuning(const void* tuning) {
    const Tuning& value = *static_cast<const Tuning*>(tuning);
    m_sim.modifyWorlds([&](World& world) { world.setTuning(value); });
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    m_camera.control(ctx.window, ctx.actions, look.camera);
    m_input.poll(ctx.actions);
    // 디버그 치트는 상태를 직접 고치지 않고 확정 명령으로 보낸다 (모든 피어가 같은 틱에 처리)
    if (ctx.actions.pressed("debug.shockwave")) sim::sendCommand(session, DebugShockwave{});
    if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // 표준 설정 화면 (HUD의 [설정] 버튼은 .ui에서 바로 settings.open)
    const bool hitStop = ctx.feedback.hitStop(); // 화면만 멈춘다: 화면 시계·효과·카메라 초점 (시뮬레이션은 계속)
    if (!hitStop) m_viewTime += dt;
    m_effects.prune(m_viewTime, look.shockwave);

    // 확정된 틱마다: 로컬 입력을 만들어 보내고, 틱 이벤트를 화면 효과로 옮긴다
    auto sample = [&](u32 tick) {
        const GameInput gi = m_input.sample(ctx.actions, m_camera);
        m_input.clearLatches();
        if (m_options.bot) {
            Headless::botCommands(tick, session.localPlayer(), session);
            return Headless::botInput(tick, session.localPlayer());
        }
        net::PlayerInput pi;
        std::memcpy(pi.bytes, &gi, sizeof(gi));
        return pi;
    };
    auto onTick = [&](const World& world) {
        bool saveNow = false;
        for (const GameEvent& e : world.events) {
            if (e.player == session.localPlayer() && e.kind == EventKind::Fall) ++m_save.falls, saveNow = true;
            if (e.player == session.localPlayer() && e.kind == EventKind::Push) ++m_save.pushes;
            m_effects.onEvent(e, m_viewTime, look.shockwave, world.tuning.pawnHalfHeight);
            ctx.feedback.trigger(e, session.localPlayer()); // 소리·흔들림·번쩍임·화면 정지는 연출 표가 정한다
        }
        if (saveNow) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    };
    m_sim.update(session, dt, sample, onTick);

    // 게임 화면 UI: 상태 → 뷰 모델, 버튼 액션 (시뮬레이션을 바꾸는 것은 확정 명령으로)
    if (m_sim.started()) fillHud(m_sim.world().state, m_sim.world().tuning, session.localPlayer(), look, m_hudVm);
    m_save.playSeconds += static_cast<f32>(dt);
    m_saveTimer += dt;
    if (m_saveTimer >= 30.0) {
        m_saveTimer = 0.0;
        game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    }
    m_hudVm.totalFalls = m_save.falls;
    m_hudVm.playMinutes = static_cast<u32>(m_save.playSeconds / 60.0f);
    for (const ui::Action& a : ctx.ui.actions())
        if (a.name == "debug.shockwave") sim::sendCommand(session, DebugShockwave{});
}

void GameSession::buildFrame(game::GameContext& ctx, f64 now, render::FrameParams& frame, render::DrawList& out) {
    const u8 localPlayer = ctx.session.localPlayer();
    const bool started = m_sim.started();
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    // 화면 정지가 시작되면 그 순간의 그림을 붙잡아 두고, 끝나면 지금 상태로 돌아온다
    const bool hitStop = started && ctx.feedback.hitStop();
    if (hitStop && !m_frozen) {
        m_frozenPrevious = m_sim.previous();
        m_frozenCurrent = m_sim.world().state;
        m_frozenAlpha = m_sim.alpha();
    }
    m_frozen = hitStop;
    const WorldState& previous = m_frozen ? m_frozenPrevious : m_sim.previous();
    const WorldState& current = m_frozen ? m_frozenCurrent : m_sim.world().state;
    const f32 alpha = m_frozen ? m_frozenAlpha : m_sim.alpha();

    // 카메라는 내가 빙의한 Pawn을 따라간다. 리스폰을 기다리는 동안은 마지막 자리에 머문다 (관전).
    if (started && localPlayer < current.playerCount) {
        const Controller* c = current.controllers.get(current.slots[localPlayer].controller);
        const Pawn* pawn = c ? current.pawns.get(c->pawn) : nullptr;
        if (pawn) {
            const Pawn* before = previous.pawns.get(c->pawn);
            m_focus = lerp(before ? before->position : pawn->position, pawn->position, alpha) +
                      Vec3{0, look.camera.focusHeight, 0};
        }
    }
    CameraTuning camera = look.camera;
    camera.fovDegrees = ctx.settings.values().display.fov; // 시야각은 플레이어 설정
    m_camera.apply(m_focus + ctx.feedback.cameraShake(m_focus), ctx.viewport, camera, frame); // 흔들림은 연출 표
    applyEnvironment(m_scene.environment, frame);

    drawScene(m_scene, ctx.models, frame.cameraPos, out);
    if (!started) return;
    drawWorld(previous, current, alpha, localPlayer, now, look, ctx.models.boxMesh(), m_view, out);
    // 캐릭터 위 이름표 (월드 위 UI): 보간한 위치 위에
    for (u32 slot = 0; slot < current.playerCount && slot < m_tagVms.size(); ++slot) {
        const Pawn* pawn = playerPawn(current, slot);
        if (!pawn) continue;
        const Controller* c = current.controllers.get(current.slots[slot].controller);
        const Pawn* before = c ? previous.pawns.get(c->pawn) : nullptr;
        const Vec3 at = lerp(before ? before->position : pawn->position, pawn->position, alpha);
        NameTagVm& tag = m_tagVms[slot];
        tag.name = "P" + std::to_string(slot + 1);
        tag.you = slot == localPlayer;
        tag.color = look.playerColors[slot];
        ctx.ui.drawAt(m_nameTag, at + Vec3{0.0f, look.nameTagHeight, 0.0f}, tag);
    }
    m_effects.draw(m_viewTime, look.shockwave, ctx.models.boxMesh(), out);
}

// 디버그 창 (개발 빌드 F1, 에디터 플레이 중). 시뮬레이션을 바꾸는 버튼은 확정 명령으로 보낸다.
void GameSession::debugUi(game::GameContext& ctx) {
#if MEALKIT_DEBUG_UI
    if (!m_sim.started()) return;
    const World& world = m_sim.world();
    const WorldState& state = world.state;
    ImGui::SetNextWindowPos(ImVec2(ctx.viewport.x + 10.0f, ctx.viewport.y + 10.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Sandbox", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("tick %u | player %u of %u", state.tick, ctx.session.localPlayer() + 1, state.playerCount);
        ImGui::Text("pawns %u | boxes %u | moving bodies %u", state.pawns.count(), state.props.count(), world.activeBodies());
        for (u32 i = 0; i < state.playerCount; ++i) ImGui::Text("P%u falls %u", i + 1, state.slots[i].falls);
        if (ImGui::Button("Shockwave (F8)")) sim::sendCommand(ctx.session, DebugShockwave{});
        ImGui::SameLine();
        ImGui::TextDisabled("sent as a command");
    }
    ImGui::End();
#else
    (void)ctx;
#endif
}

} // namespace sandbox
