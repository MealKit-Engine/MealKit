#pragma once

#include "app/Flow.h"
#include "app/ProjectSettings.h"
#include "app/SaveData.h"
#include "sim/World.h"
#include "view/Camera.h"
#include "view/Draw.h"
#include "view/Effects.h"
#include "view/Hud.h"
#include "view/LocalInput.h"
#include "view/Prediction.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/ModelCache.h"
#include "mealkit/audio/Audio.h"
#include "mealkit/game/SimulationGame.h"
#include "mealkit/session/Session.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <array>
#include <string>
#include <vector>

// Running one match: runs the simulation (sim/) every confirmed tick, sends local input, and draws the view (view/).
// The shipping game (main.cpp) uses this class directly, and the general-purpose editor uses it through the game DLL (GameModule.cpp).
namespace sandbox {

class GameSession final : public mealkit::game::SimulationGame<World> {
public:
    using ProjectSettingsType = ProjectSettings; // game items in the editor [Project Settings] window
    using ViewTuningType = ViewTuning;           // view tuning (assets/view.tuning, "View" tab of the [Tuning] panel)
    using EventType = GameEvent;                 // tick events ([Feedback] panel, assets/feedback.table)

    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    void start(mealkit::game::GameContext& ctx, const mealkit::scene::Scene& scene,
               const mealkit::game::PlayOptions& options) override;
    void stop(mealkit::game::GameContext& ctx) override;
    bool menus() const override { return true; } // title/menu/lobby/results screens (app/Flow.cpp)
    void update(mealkit::game::GameContext& ctx, mealkit::f64 dt) override;
    void buildFrame(mealkit::game::GameContext& ctx, mealkit::f64 now, mealkit::render::FrameParams& frame,
                    mealkit::render::DrawList& out) override;
    void lobby(mealkit::session::Session& session, const mealkit::game::PlayOptions& options,
               const std::string& userFolder) override;
    void loadSave(const std::string& userFolder);
    void debugUi(mealkit::game::GameContext& ctx) override;

private:
    Flow m_flow; // screen flow (menu, lobby, pause, results)
    mealkit::scene::Scene m_scene;                       // the flattened match scene (also drawn behind menus)
    const mealkit::scene::Scene* m_sceneSource = nullptr; // the source m_scene was made from
    mealkit::game::PlayOptions m_options;
    GameCamera m_camera;     // virtual cameras of the match scene + cutscenes
    GameCamera m_menuCamera; // behind the menu/lobby
    std::vector<std::string> m_cameraEvents; // cutscene feedback rows fired this frame
    MatchPhase m_lastPhase = MatchPhase::Waiting;
    LocalInput m_input;
    LocalPrediction m_prediction; // moves only my character's appearance ahead (hides input delay)
    WorldView m_view;
    std::vector<mealkit::render::ReflectionProbe> m_probes; // reflection probes (filled every frame)
    std::vector<mealkit::render::PointLight> m_lights;      // point lights (filled every frame)
    mealkit::f64 m_cameraTime = 0.0, m_menuTime = 0.0; // last buildFrame times (camera dt)
    Effects m_effects;              // tick events → view effects
    mealkit::f64 m_viewTime = 0.0;  // clock for view effects (sum of update dt)
    // Game screen UI (assets/ui): HUD and name tags above characters (world-space UI)
    mealkit::u32 m_hud = 0, m_nameTag = 0;
    HudVm m_hudVm;
    // Progress data (saves/progress.save): saved at match start, on falls, and every 30 seconds
    SaveData m_save;
    bool m_saveLoaded = false;
    mealkit::f64 m_saveTimer = 0.0;
    std::array<NameTagVm, mealkit::session::kMaxPlayers> m_tagVms; // must live until drawAt draws them
    // State drawn during hitstop: the two tick states and interpolation fraction at the moment it started (the simulation keeps running)
    bool m_frozen = false;
    WorldState m_frozenPrevious, m_frozenCurrent;
    f32 m_frozenAlpha = 1.0f;
};

} // namespace sandbox
