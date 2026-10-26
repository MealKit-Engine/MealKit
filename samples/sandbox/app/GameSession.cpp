#include "app/GameSession.h"
#include "app/Headless.h"
#include "app/Loadout.h"
#include "sim/Commands.h"

#include "mealkit/feedback/Feedback.h"
#include "mealkit/game/GameData.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/game/Save.h"
#include "mealkit/loc/Strings.h"
#include "mealkit/platform/Window.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/sim/Commands.h"
#include "mealkit/ui/Ui.h"

#if MEALKIT_DEBUG_UI
#include <imgui.h>
#endif

#include <cstring>

using namespace mealkit;

namespace sandbox {

void GameSession::loadSave(const std::string& userFolder) {
    if (m_saveLoaded) return;
    game::readSave(m_save, userFolder, "progress", kSaveVersion);
    m_saveLoaded = true;
}

// 매치 전: 로드아웃 (봇은 플레이어마다 다른 몸, 사람은 로비에서 고른 몸 = 세이브). 로비 화면이 몸을 고른다 (app/Flow.cpp)
void GameSession::lobby(net::LockstepSession& session, const game::PlayOptions& options, const std::string& userFolder) {
    if (options.bot) {
        Headless::botLoadout(session.localPlayer(), session);
        return;
    }
    loadSave(userFolder);
    Loadout l;
    l.body = m_save.body;
    game::setLoadout(session, l);
}

void GameSession::start(game::GameContext& ctx, const scene::Scene& scene, const game::PlayOptions& options) {
    const net::LockstepSession& session = ctx.session;
    m_scene = scene::flattened(scene); // 부모·자식 계층을 월드 좌표로 펼친다 (시뮬레이션과 그리기는 평평한 목록만 본다)
    m_sceneSource = &scene;
    m_options = options;
    Tuning tuning;
    game::loadTuning(reflect::typeOf<Tuning>(), &tuning, options.tuningPath);
    const game::MatchInfo match = game::matchInfo(session); // 시드, 인원, 매치 데이터, 플레이어별 로드아웃
    m_sim.start(options, game::tickDt(), [&](World& world, u32 physicsThreads) {
        Headless::startWorld(world, match, tuning, m_scene, ctx.models, physicsThreads); // 헤드리스와 같은 매치 시작
    });
    m_input.clearLatches();
    m_prediction.reset();
    m_effects = {};
    m_hudVm = {};
    ctx.audio.stop(m_music);
    m_music = ctx.audio.play2D("music");
    loadSave(ctx.userFolder);
    ++m_save.matchesStarted;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!ctx.ui.isOpen(m_hud)) m_hud = ctx.ui.open("ui/hud.ui");
    if (!ctx.ui.isOpen(m_nameTag)) m_nameTag = ctx.ui.openWorld("ui/nametag.ui");
    ctx.ui.bind(m_hud, m_hudVm);
}

// 매치 끝 (로비로 돌아감, 나감, 끊김): World를 버리고 매치 화면을 닫는다. 세이브는 남긴다
void GameSession::stop(game::GameContext& ctx) {
    m_sim.stop();
    ctx.audio.stop(m_music);
    if (m_hud) ctx.ui.close(m_hud);
    if (m_nameTag) ctx.ui.close(m_nameTag);
    m_hud = m_nameTag = 0;
    m_flow.stop(ctx);
    m_frozen = false;
    game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
}

void GameSession::setTuning(const void* tuning) {
    const Tuning& value = *static_cast<const Tuning*>(tuning);
    m_sim.modifyWorlds([&](World& world) { world.setTuning(value); });
}

void GameSession::update(game::GameContext& ctx, f64 dt) {
    net::LockstepSession& session = ctx.session;
    const ViewTuning& look = ctx.viewTuning<ViewTuning>();
    loadSave(ctx.userFolder);
    // 화면 흐름: 세션 단계에 맞는 화면을 열고 버튼을 처리한다 (메뉴·로비·일시정지·결과·채팅)
    m_flow.update(ctx, m_sim.started() ? &m_sim.world() : nullptr, m_save, m_hudVm, m_hud, dt);
    if (m_flow.takeSaveChanged()) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    if (!m_sim.started()) {
        if (ctx.scene && ctx.scene != m_sceneSource) { // 메뉴 뒤에 보일 경기장
            m_scene = scene::flattened(*ctx.scene);
            m_sceneSource = ctx.scene;
        }
        return;
    }

    const bool menuOpen = m_flow.blocksInput(); // 일시정지·채팅·결과 중: 캐릭터는 서 있고 카메라도 그대로
    if (!menuOpen) {
        m_camera.control(ctx.window, ctx.actions, look.camera);
        m_input.poll(ctx.actions);
        // 디버그 치트는 상태를 직접 고치지 않고 확정 명령으로 보낸다 (모든 피어가 같은 틱에 처리)
        if (ctx.actions.pressed("debug.shockwave")) sim::sendCommand(session, DebugShockwave{});
        if (ctx.actions.pressed("menu.settings")) ctx.settings.open(); // 표준 설정 화면 (HUD의 [설정] 버튼은 .ui에서 바로 settings.open)
    } else {
        ctx.window.setCursorCaptured(false);
        m_camera.orbiting = false;
    }
    const bool hitStop = ctx.feedback.hitStop(); // 화면만 멈춘다: 화면 시계·효과·카메라 초점 (시뮬레이션은 계속)
    if (!hitStop) m_viewTime += dt;
    m_effects.prune(m_viewTime, look.shockwave);

    // 확정된 틱마다: 로컬 입력을 만들어 보내고, 틱 이벤트를 화면 효과로 옮긴다
    auto sample = [&](u32 tick) {
        GameInput gi = m_input.sample(ctx.actions, m_camera);
        if (m_flow.blocksInput()) gi = GameInput{};
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
            onWorldEvent(e, world.state, m_viewTime, m_view); // 밀쳐진 캐릭터 번쩍임
            ctx.feedback.trigger(e, session.localPlayer()); // 소리·흔들림·번쩍임·화면 정지는 연출 표가 정한다
        }
        if (saveNow) game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    };
    if (!m_flow.pausesSimulation()) m_sim.update(session, dt, sample, onTick); // 혼자 하기의 일시정지는 매치를 멈춘다

    // 내 캐릭터 미리 움직이기: 보냈지만 아직 실행되지 않은 내 이동 입력만큼 (화면만, 경기 중에만)
    const World& world = m_sim.world();
    const u8 me = session.localPlayer();
    m_prediction.begin();
    m_sim.forEachPendingInput([&](u32, const net::PlayerInput& in) { m_prediction.addInput(world, me, in); });
    m_prediction.update(static_cast<f32>(dt), look.prediction, world.state.phase == MatchPhase::Playing && playerPawn(world.state, me));

    // 게임 화면 UI: 상태 → 뷰 모델, 버튼 액션 (시뮬레이션을 바꾸는 것은 확정 명령으로)
    std::array<std::string, net::kMaxPlayers> names;
    for (u32 i = 0; i < names.size(); ++i) names[i] = Flow::playerName(session, i);
    fillHud(m_sim.world().state, m_sim.world().tuning, session.localPlayer(), look, names, m_hudVm);
    m_save.playSeconds += static_cast<f32>(dt);
    m_saveTimer += dt;
    if (m_saveTimer >= 30.0) {
        m_saveTimer = 0.0;
        game::writeSave(m_save, ctx.userFolder, "progress", kSaveVersion);
    }
    // 플레이어용 네트워크 표시 (플레이어 설정으로 끈다): 플레이어별 핑, 입력을 기다리며 멈추면 누구 때문인지
    const bool showNet = ctx.settings.values().gameplay.showNetwork && !session.isOffline();
    for (u32 i = 0; i < m_hudVm.playersCount; ++i) {
        PlayerRowVm& row = m_hudVm.players[i];
        row.ping = session.pingMs(i);
        row.pingBad = showNet && i != 0 && row.ping >= look.badPingMs;
        row.pingGood = showNet && i != 0 && !row.pingBad;
    }
    m_hudVm.waiting.clear();
    if (const u32 mask = showNet ? session.waitingFor() : 0u) {
        std::string list;
        for (u32 i = 0; i < m_hudVm.playersCount; ++i)
            if (mask & (1u << i)) list += (list.empty() ? "" : ", ") + names[i];
        m_hudVm.waiting = ctx.strings.format("hud.waiting", {{"names", list}});
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
    if (!started) { // 메뉴·로비 뒤: 경기장을 천천히 돌며 보여 준다
        CameraTuning camera = look.camera;
        camera.fovDegrees = ctx.settings.values().display.fov;
        OrbitCamera menuCamera;
        menuCamera.yaw = look.camera.startYaw + static_cast<f32>(now) * look.menuOrbitSpeed;
        menuCamera.pitch = look.camera.startPitch;
        menuCamera.distance = look.camera.startDistance * 1.6f;
        menuCamera.apply(Vec3{0.0f, look.camera.focusHeight, 0.0f}, ctx.viewport, camera, frame);
        applyEnvironment(m_scene.environment, ctx.models, frame);
        applyProbes(m_scene, ctx.models, m_probes, frame);
        applyLights(m_scene, m_view, m_viewTime, m_lights, frame);
        drawScene(m_scene, ctx.models, frame.cameraPos, out);
        return;
    }
    // 화면 정지가 시작되면 그 순간의 그림을 붙잡아 두고, 끝나면 지금 상태로 돌아온다
    const bool hitStop = ctx.feedback.hitStop();
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
    if (localPlayer < current.playerCount) {
        const Controller* c = current.controllers.get(current.slots[localPlayer].controller);
        const Pawn* pawn = c ? current.pawns.get(c->pawn) : nullptr;
        if (pawn) {
            const Pawn* before = previous.pawns.get(c->pawn);
            m_focus = lerp(before ? before->position : pawn->position, pawn->position, alpha) + m_prediction.offset +
                      Vec3{0, look.camera.focusHeight, 0};
        }
    }
    CameraTuning camera = look.camera;
    camera.fovDegrees = ctx.settings.values().display.fov; // 시야각은 플레이어 설정
    m_camera.apply(m_focus + ctx.feedback.cameraShake(m_focus), ctx.viewport, camera, frame); // 흔들림은 연출 표
    applyEnvironment(m_scene.environment, ctx.models, frame);
    applyProbes(m_scene, ctx.models, m_probes, frame);
    applyLights(m_scene, m_view, m_viewTime, m_lights, frame);

    drawScene(m_scene, ctx.models, frame.cameraPos, out);
    drawWorld(previous, current, alpha, localPlayer, now, m_viewTime, look, ctx.models.boxMesh(), m_view, out, m_prediction.offset);
    // 캐릭터 위 이름표 (월드 위 UI): 보간한 위치 위에
    for (u32 slot = 0; slot < current.playerCount && slot < m_tagVms.size(); ++slot) {
        const Pawn* pawn = playerPawn(current, slot);
        if (!pawn) continue;
        const Controller* c = current.controllers.get(current.slots[slot].controller);
        const Pawn* before = c ? previous.pawns.get(c->pawn) : nullptr;
        const Vec3 at = lerp(before ? before->position : pawn->position, pawn->position, alpha) +
                        (slot == localPlayer ? m_prediction.offset : Vec3{});
        NameTagVm& tag = m_tagVms[slot];
        tag.name = Flow::playerName(ctx.session, slot);
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
        ImGui::Text("tick %u | player %u of %u | phase %u", state.tick, ctx.session.localPlayer() + 1, state.playerCount,
                    static_cast<u32>(state.phase));
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
