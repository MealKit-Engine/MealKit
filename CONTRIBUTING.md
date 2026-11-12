# Contributing to MealKit

Thanks for helping. MealKit is in beta (v0.x): the `GameInstance` API and the file formats may still change until v1.0. The full guide for working on the engine is in the documentation ("Engine developer guide"); this file holds the rules that change together with the code.

## Before you write code

- Read the plan in [`docs/ROADMAP.md`](docs/ROADMAP.md), the decisions and the "Deferred / not included" table in [`docs/DECISIONS.md`](docs/DECISIONS.md), and the implementation answers in [`docs/CODING_MODEL.md`](docs/CODING_MODEL.md) ("one answer per problem").
- A design question that isn't answered there starts as an issue. It gets a row in the decision log, agreed on before code is written.
- A feature is done when it is used in a reference sample (`samples/sandbox`, `samples/coop`). When it is done, it is removed from `docs/ROADMAP.md` (which lists only work that is left) and the details go to `docs/CHANGELOG.md`.

## Developer Certificate of Origin

Contributions are accepted under the [Developer Certificate of Origin](https://developercertificate.org/); there is no CLA. Sign off every commit with `git commit -s` (a `Signed-off-by: Name <email>` line), which states that you may submit the change under this repository's license: code MIT, sample assets CC0 (except the fonts, SIL OFL; see `samples/*/ASSETS.md`).

## Absolute rules

- Always 3D and multiplayer by default: `net` is an engine module in the default preset, and determinism rules hold for every game, with or without it. The default preset stays light and fast; features over its budget are opt-in with no GPU cost when off.
- OS APIs are called only inside `engine/core/platform/src/mealkit/platform/<os>/`, graphics APIs only inside `modules/rhi/src/mealkit/rhi/<api>/` (`tools\check_platform.ps1`). The renderer uses only `rhi::Device`/`CommandList`, and shader bindings have one shape in `shaders/Bindless.hlsli`. DX12 is the reference backend; a graphics feature is done when it works on Vulkan too. No external RHI libraries, no OpenGL/DX11.
- Console SDKs and code built on them (NDA) never go in this repository.
- The engine builds only the deterministic simulation, lockstep networking, the renderer and the editor; everything else uses finished external tools and libraries. Workflows with no suitable free tool become separate MealKit tool repositories (MIT).
- Simulation code (`samples/*/sim/`, a project's `source/sim/`, `modules/physics`) follows the determinism rules in `docs/CODING_MODEL.md`: no CRT trigonometry (`mealkit/core/DetMath.h`), `/fp:precise`, only `Pcg32` for randomness, fixed iteration and body creation order, and state that lives across ticks is a `MEALKIT_FIELD` inside `WorldState`. Game `sim/` is checked by SimCheck at build time.
- Don't change `CROSS_PLATFORM_DETERMINISTIC=ON` or the disabled AVX/FMA in the Jolt options (`cmake/MealKit.cmake`). Clang/GCC use `-ffp-contract=off`; `-ffast-math` is forbidden. No platform branches (`#ifdef`, per-platform libraries) in simulation code.

## Code style

- C++20, `.clang-format` at the root. Comments, log messages and scene file keys are English.
- Editor text is English source in `tr("...")`/`trl()`/`trf()` with the Korean translation in `engine/res/strings/ko.txt` (`tools\check_strings.ps1`).
- When usage changes, update the documentation repository (MealKit-docs) in English and Korean in the same change.

## Build

Windows with Visual Studio 2022 (or Build Tools) with C++:

```bat
build.bat            :: Release: build\Release\bin (sandbox.exe, coop.exe, headless, tests)
build.bat Editor     :: build\Editor\bin\MealKitEditor.exe
build.bat Export     :: shipping folders in samples\<name>\export
```

Other configurations: `Debug`, `Profile` (Tracy), `ClangCL`, `Vulkan`, `VulkanEditor`. Linux/macOS currently build the headless targets: `pwsh build.ps1 Headless`. To check that build on Windows before pushing (core only, clang-cl, as CI builds it on Linux/macOS): `build.bat HeadlessOnly`, then `tools\ci_test.ps1 -Bin build\HeadlessOnly\bin -Name local -Out build\ci-local`.

## Checks

Run the checks that match what you changed:

| Changed | Run |
|---|---|
| Docs, `*.md`, comments only | nothing |
| Engine, sample or tool code | `powershell -ExecutionPolicy Bypass -File tools\check.ps1 -Changed` (preview with `-DryRun`) |
| One area | `tools\check.ps1 -Only render,editor` or `build\Release\bin\mealkit_tests.exe --only net,physics` |

- Run `check.ps1` on its own (not while building or editing) and with the editor closed; logs are in `build\check\`.
- If you change simulation results on purpose, re-record `samples/*/replays/*.rec` with `tools\record_replay.ps1`, update scenarios with `<sample>_headless --test <file> --update-expected`, and commit them together.
- Multiplayer: `tools\mp_test.ps1 -Clients 3 -Ticks 900` (`-Headless`, distant peers `-Lag 120 -Jitter 30 -Loss 3`).
- CI (`.github/workflows/ci.yml`) builds Windows MSVC/Editor/clang-cl and Linux/macOS headless, and compares per-tick hashes across them.

## Adding an external library

A runtime library must be verified as deterministic if it touches the simulation, have a permissive license (MIT, BSD, zlib, public domain and similar) and fit in the 5 MB budget of the shipped executable.
Copy it into `third_party/<lib>/` at a fixed version with its `LICENSE` and a `mealkit.txt`, then regenerate `third_party/VERSIONS.txt` and `THIRD_PARTY_NOTICES.txt` with `tools\gen_notices.ps1` (never edit those two by hand).

## Reporting

- Bugs: open an issue and attach the bug report zip the game saves with F9 (its `info.txt`, log and input recording let anyone replay it).
- Security problems: see [`SECURITY.md`](SECURITY.md); don't open a public issue.
