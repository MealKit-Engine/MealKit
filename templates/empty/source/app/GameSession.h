#pragma once

#include "app/Flow.h"
#include "app/ProjectSettings.h"
#include "app/SaveData.h"
#include "sim/World.h"
#include "view/Camera.h"
#include "view/Draw.h"
#include "view/Hud.h"
#include "view/LocalInput.h"
#include "view/ViewTuning.h"

#include "mealkit/game/SimulationGame.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <string>
#include <vector>

// Running one match: runs the simulation (sim/) every confirmed tick, sends local input, and draws the view (view/).
// The shipping game (main.cpp) uses this class directly, and the general-purpose editor uses it through the game DLL (GameModule.cpp).
namespace sandbox {

class GameSession final : public mealkit::game::SimulationGame<World> {
public:
    using ProjectSettingsType = ProjectSettings; // game items in the editor [Project Settings] window
    using ViewTuningType = ViewTuning;           // assets/view.tuning, "View" tab of the [Tuning] panel
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

private:
    void loadSave(const std::string& userFolder);

    Flow m_flow;                                          // screen flow (menu, lobby, pause, results)
    mealkit::scene::Scene m_scene;                        // the flattened match scene (also drawn behind menus)
    const mealkit::scene::Scene* m_sceneSource = nullptr; // the source m_scene was made from
    mealkit::game::PlayOptions m_options;
    GameCamera m_camera;     // the match: virtual cameras of the scene, or following my character
    GameCamera m_menuCamera; // behind the menu/lobby
    LocalInput m_input;
    std::vector<mealkit::render::ReflectionProbe> m_probes; // filled every frame
    std::vector<mealkit::render::PointLight> m_lights;      // filled every frame
    mealkit::f64 m_cameraTime = 0.0, m_menuTime = 0.0;      // last buildFrame times (camera dt)
    mealkit::f64 m_viewTime = 0.0;                          // clock for view animation (sum of update dt)
    mealkit::u32 m_hud = 0;
    HudVm m_hudVm;
    SaveData m_save; // saves/progress.save
    bool m_saveLoaded = false;
};

} // namespace sandbox
