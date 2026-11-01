#pragma once

#include "sim/World.h"
#include "view/Camera.h"

#include "mealkit/input/Actions.h"

// Local controls (input actions move/jump/push, assets/input.bindings) → GameInput (8 bytes).
// The move direction is computed relative to the camera and quantized as a world direction.
// This is outside the simulation, so trig is fine (only the resulting bytes are sent).
namespace sandbox {

struct LocalInput {
    bool pushLatch = false;
    bool jumpLatch = false;

    // Every frame: remembers buttons briefly pressed between ticks until the next tick
    void poll(const mealkit::input::Actions& actions);
    // Every tick: turns the current controls into input. Call clearLatches() after
    GameInput sample(const mealkit::input::Actions& actions, const GameCamera& camera) const;
    void clearLatches() { pushLatch = jumpLatch = false; }
};

} // namespace sandbox
