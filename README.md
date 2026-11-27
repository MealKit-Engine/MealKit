# MealKit

A lightweight 3D multiplayer game engine: a light default that runs fast even on integrated graphics, with heavier features opt-in per project.

> **Beta (v0.x).** MealKit is under active development: APIs and file formats may change between versions, and projects are moved forward with the upgrade tool. Feedback and bug reports are welcome.

## Features

- **Deterministic lockstep multiplayer**: only 8 bytes of input per player per tick go over the wire, so bandwidth doesn't depend on object count (about 5 KB/s for a 4-player host with 8,000 physics boxes). Desyncs are caught by per-tick state hashes, and up to 8 players can join over UDP or Steam relay.
- **Deterministic physics**: Jolt Physics in its cross-platform deterministic build, plus a build-time checker (SimCheck) that rejects non-deterministic code in game simulation.
- **Fast on integrated graphics**: a forward renderer (DirectX 12, Vulkan, WebGPU) with per-mesh instancing and automatic LODs runs at about 3,500 fps (2,000 boxes, 4 draw calls) on Intel Core Ultra 5 125U integrated graphics.
- **Tiny builds**: a shipped game is a single static exe of about 4.3 MB (5 MB budget, checked in CI), with no redistributable needed; the browser build's `.wasm` is about 3.7 MB.
- **Godot-style editor**: one general-purpose editor opens many game project folders. Game code is C++ and hot-reloads into the editor as a DLL, and one-click export makes a release folder.

Windows (DirectX 12 or Vulkan), Linux (Vulkan or WebGPU) and the browser (WebGPU; multiplayer through a relay server, `tools/relay.py`, also with Linux builds); macOS and consoles are planned. GPU: Shader Model 6.6 bindless (DirectX 12) or Vulkan 1.3; no other minimum spec.

Documentation: the separate `MealKit-Docs` repository (English and Korean, published together with this repository) · [Roadmap](docs/ROADMAP.md) · [Decisions](docs/DECISIONS.md) · [Changelog](docs/CHANGELOG.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) (rules, build, checks, DCO sign-off). Security problems: [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE). Third-party licenses: [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
