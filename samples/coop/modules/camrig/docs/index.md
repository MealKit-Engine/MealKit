# Camera rigs

Two ways for a player to move the game camera. Both are view-side only: nothing here enters the simulation.

- Version 0.1.0, engine 0.9, requires the engine modules `camera`, `input`, `render`
- `view/Orbit.h`: `camrig::OrbitControl`, turning and zooming the engine's `camera::Brain` (third person spring arm, the scene's virtual cameras, cutscenes)
- `view/Side2D.h`: `camrig::Side2DCamera`, an orthographic side view on the XY plane that follows a point (`mealkit.project` `view2d`)

A game with a fixed camera, a camera placed in the scene, or its own rules needs neither: keep a few lines in your `source/view/` instead.

## Adding

```
tools\module.ps1 add -Project <your project> camrig
```

Input actions read from `assets/input.bindings`: `orbit` (hold to turn with the mouse), `look` (mouse movement), `turn` (gamepad stick) and `zoom` (wheel). The side camera reads only `zoom`.

## Orbit

```cpp
// source/view/Camera.h
#include "camrig/view/Orbit.h"

struct GameCamera {
    mealkit::camera::Brain brain;
    camrig::OrbitControl orbit;
};

// every frame, when no menu blocks input
cam.orbit.control(ctx.window, ctx.actions, {tuning.orbitSensitivity, tuning.zoomStep});
// menus open
cam.orbit.release(ctx.window);

// before drawing: target = the followed character's position (nullptr while there is none)
mealkit::camera::BrainInput in = cam.orbit.take(dt, target, fov);
in.shake = ctx.feedback.cameraShake();
in.ray = [&](mealkit::Vec3 from, mealkit::Vec3 to, mealkit::f32& fraction) { /* wall query for the spring arm */ return false; };
cam.brain.update(in);
cam.brain.apply(viewport, nearPlane, farPlane, frame);
```

`brain.flatForward()` is the horizontal forward direction for turning move input into world directions.

## Side view (2D)

```cpp
#include "camrig/view/Side2D.h"

camrig::Side2DCamera cam;     // cam.reset() at match start
camrig::Side2DSettings rig;   // or a field of your view tuning

cam.control(ctx.actions, rig);              // zoom
cam.update(target, dt, rig);                // follow (nullptr = stay)
cam.apply(ctx.viewport, rig, frame);        // orthographic view → frame
```

## Version history

- 0.1.0: first version, from the engine 0.9.7 samples' orbit input and the `empty2d` template's follow camera
