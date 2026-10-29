# Visual feedback ("보는맛") — design draft

Status: **draft, not decided** (2026-10-29). Nothing here is in ROADMAP yet. When the user picks items, they move to `docs/ROADMAP.md` as items and the open questions below become rows in `docs/DECISIONS.md`.

## Goal

The feedback table (`assets/feedback.table`, `feedback::Reaction`) already gives "손맛" without code: one row per event kind gives sound, camera shake, screen flash, hit stop and a particle effect.
The visual side stops at a whole-screen flash and particles. The renderer already has the parts for more (point lights in `FrameParams::lights`, the instance flash bits 16–23, emissive, rim, dissolve, bloom), but a designer can only reach them by writing view code.

Goal: **a designer adds light and glow to an event with one table row**, the same way as shake. Same rules as the existing columns:

- Strength from `strengthFrom` / `strengthFull` scales every visual column.
- Position from `positionFrom`; the range column fades it with distance.
- View side only. The simulation never reads any of it, so determinism and the content hash are untouched.
- Player settings scale it (accessibility: a "flashing lights" slider next to flash, see below).
- Works in the editor [Feedback] panel preview without play.

## Part 1 — new feedback table columns

### 1. Event light (`light*`)

A point light that appears at the event position and fades out. Muzzle flash, explosion, spell hit, pickup.

| Field | Default | Meaning |
|---|---|---|
| `light` | 0 | Intensity at full strength (0 = no light) |
| `lightColor` | white | Color (sRGB in the table, squared to linear like `flashColor`) |
| `lightRange` | 4 | Range (m) at full strength |
| `lightSeconds` | 0.15 | Time to fade out |
| `lightCurve` | `fast` | `linear`, `fast` (1−t)², `flicker` (fade × noise, for fire) |
| `lightOffset` | 0,0.5,0 | Added to the event position, so the light is not inside the floor |

Implementation:

- `feedback::Player` keeps up to N active lights (N = 32, oldest/weakest replaced). `Player::addLights(std::vector<render::PointLight>&)` appends them; `game::Frontend` calls it after `buildFrame`, the same place it writes `tintColor`.
- The renderer already picks the 4 strongest lights per instance on the CPU, so many short lights cost only the CPU pick. No shader change.
- Point lights have no shadows. That is fine for 0.1–0.3 s lights.
- Hit stop: lights freeze with the view (they use view time, like shakes).

### 2. Object hit flash (`hitFlash*`)

The hit object turns white for a moment. The classic "it got hit" read, cheaper and clearer than a screen flash.

| Field | Default | Meaning |
|---|---|---|
| `hitFlash` | 0 | Flash amount 0..1 at full strength |
| `hitFlashSeconds` | 0.1 | Time |
| `targetFrom` | `target` | Event field with the entity id of the object that flashes |

Implementation:

- The table does not know which instance is which entity. `Player` keeps a small map entity id → (amount, end time). View code asks `ctx.feedback.objectFlash(entityId)` when it adds the instance and passes the result to `render::instanceFlash(flags, amount)`, which already exists.
- The `SceneView` / default actor drawing does this automatically, so games that use the engine's drawing need no code.
- Open question 1: should the flash color be a column (bits only hold an amount toward white now)? A red "damage" flash needs 3 more bytes per instance or a small per-frame color table indexed by 4 bits.

### 3. Glow kick (`bloomKick*`, `exposureKick*`)

A short rise of bloom (or exposure) so a big hit "burns" the screen for a moment, softer than a full flash.

| Field | Default | Meaning |
|---|---|---|
| `bloomKick` | 0 | Added to bloom intensity at full strength |
| `exposureKick` | 0 | Added to exposure (stops) at full strength, can be negative (a dark "suck in" before an explosion) |
| `kickSeconds` | 0.2 | Time |

Implementation: `Player::postKick(f32& bloom, f32& exposure)` like `screenTint`, strongest wins. `FrameParams` gets two offsets that the uber pass adds. Cost: zero (uniforms).

### 4. Object glow pulse (`emissivePulse*`)

The hit or activated object's emissive and rim light up and fade: a charged weapon, a button that was pressed, a pickup that was touched.

| Field | Default | Meaning |
|---|---|---|
| `emissivePulse` | 0 | Emissive multiplier added at full strength |
| `emissivePulseSeconds` | 0.3 | Time |

Uses the same `targetFrom` entity. Needs a per-instance emissive scale (open question 2: share the byte with flash or add a word to the instance data; the instance is 64 bytes now).

### 5. Shockwave distortion (`shockwave*`) — later, needs Part 2 item D

A ring that bends the picture outward from the event position.

| Field | Default | Meaning |
|---|---|---|
| `shockwave` | 0 | Bend strength at full strength |
| `shockwaveRadius` | 6 | Final ring radius (m) |
| `shockwaveSeconds` | 0.35 | Time |

### Accessibility

- A new player setting **Flashing lights** (0..1) multiplies `light`, `hitFlash`, `bloomKick`, `exposureKick` and the existing `flash`. The existing flash setting is merged into it (open question 3: one slider or two).
- `shockwave` follows the shake setting (it is motion).
- Photosensitivity guard: the `Player` limits large brightness changes to 3 per second over the whole screen (WCAG 2.3.1 style), whatever the table says.

### Editor

- [Feedback] panel: the new columns appear by reflection with no work. The preview button fires them at the look dev turntable's object, so `hitFlash`/`emissivePulse` have a target.
- View > View Mode > **Event Lights**: draws each active event light as a wire sphere with its range.

## Part 2 — shading techniques worth adding

Each one is a toggle with the integrated graphics preset deciding the default, like stage 2. Cost is a guess for the minimum spec (11th gen Xe, 1280×720); measure with `--benchmark` before deciding.
Ordered by how much "보는맛" per millisecond.

| # | Technique | What it gives | Rough cost | Notes |
|---|---|---|---|---|
| A | **Decals** (projected boxes) | Scorch marks, blood, footprints, bullet holes, blob shadows under characters. Events leave traces. | 0.2–0.5 ms | Also a feedback column `decal` (texture, size, lifetime). Box projection in the forward pass via a decal list like point lights (up to 4 per instance), so no G-buffer is needed. |
| B | **Screen space ambient occlusion** (half resolution GTAO or simple SAO) | Objects sit on the ground, corners get depth. The biggest "it looks real" step after shadows. | 0.6–1.0 ms half res | Needs a depth prepass or the depth of the main pass; Toon can use it as a band darkener. |
| C | **Soft particles** | Particles stop cutting hard lines into the floor and walls. | ~0.1 ms | Sample scene depth in the particle shader and fade near surfaces. Check whether `softness` in `.fx` already does this; if it is only edge softness, add depth fade. |
| D | **Screen distortion pass** | Heat haze over fire, shockwave rings, water refraction. | 0.2 ms | One extra full-screen sample in the uber pass from a small "distortion" target that particles/shockwaves draw into. Enables Part 1 item 5. |
| E | **Height fog + sun light shafts** (screen space god rays) | Atmosphere, depth, dramatic sun through trees. | 0.3–0.5 ms | Height fog is a small change in the existing fog. Light shafts: radial blur of a bright mask from the sun position at quarter resolution. |
| F | **Animated emissive** (glTF `extras`: pulse speed, scroll UV) | Glowing runes, screens, lava, warning lights without animation keys. | ~0 | Material feature like rim; Blender add-on panel field. |
| G | **Spot lights + light cookies** | Flashlights, stage lights, patterned light through windows. | like point lights | Same 4-per-instance table, plus a cone and an optional cookie texture index. Still no shadows. |
| H | **Edge-detect outline** (depth + normal, post pass) | A cleaner outline for Toon than the inverted hull, including inner lines, with no per-material setting. | 0.3 ms | Keep the hull outline for per-material color; this is an option on top. |
| I | **Screen space contact shadows** | Small, sharp shadows where the sun's shadow map is too coarse (feet, hands on tables). | 0.3 ms | A short ray march in depth toward the sun, only near the camera. |
| J | **Subsurface wrap / skin shading** | Softer skin and wax, light bleeding at edges. | ~0 | A wrap term + tint color per material (extras). |
| K | **Hatching / stylized shadows** | A drawn look for Toon (lines in shadow areas). | ~0 | Screen space or triplanar hatch texture driven by the toon ramp value. |

Not recommended for the minimum spec now: screen space reflections, TAA (FXAA + CAS is the decision), volumetric fog with froxels, ray tracing, deferred shading. Probes and the reflection cube cover reflections.

## Suggested order

1. Part 1 items 1 (event light), 2 (hit flash), 3 (glow kick) — all reuse existing renderer parts, small change, biggest effect for designers.
2. Part 2 A (decals, with a `decal` feedback column) and C (soft particles).
3. Part 2 B (SSAO) and F (animated emissive).
4. Part 2 D (distortion) → Part 1 item 5 (shockwave).
5. The rest when a game needs them.

## Open questions (become DECISIONS rows)

1. Hit flash color: white only, or a color (instance data size)?
2. Emissive pulse: where the per-instance scale lives.
3. One "flashing lights" accessibility slider replacing the flash slider, or two sliders.
4. Event light limit N (32?) and whether event lights count against `pointLightMax`-style project settings.
5. Do decals belong in the feedback table (event traces) only, or also as placed scene objects?
