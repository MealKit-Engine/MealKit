#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/net/Lockstep.h"

// Moves only my character's appearance ahead (ROADMAP stage 1 "Predict only my character's appearance"): during input delay (tens to hundreds of ms)
// my character is drawn ahead by my movement inputs sent but not yet executed. The simulation is unchanged (unrelated to determinism),
// and as the confirmed state catches up the difference shrinks and blends smoothly. Only horizontal movement is predicted; jumps/pushes/collisions with other characters show after confirmation.
namespace sandbox {

struct LocalPrediction {
    Vec3 offset; // the difference drawn now (relative to the confirmed position)

    // Every frame: receives the movement to add this time (sum of not yet executed inputs) and smoothly moves offset toward it
    void begin() { m_target = {}; }
    void addInput(const World& world, u8 player, const mealkit::net::PlayerInput& input);
    void update(f32 dt, const PredictionLook& look, bool active);
    void reset() { offset = m_target = {}; }

private:
    Vec3 m_target;
};

} // namespace sandbox
