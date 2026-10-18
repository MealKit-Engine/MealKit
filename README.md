# MealKit

A lightweight 3D multiplayer game engine that runs fast even on office PCs with Intel integrated graphics.

## Features

- **Deterministic lockstep multiplayer**: only 8 bytes of input per player per tick go over the wire, so bandwidth doesn't depend on object count (about 5 KB/s for a 4-player host with 8,000 physics boxes). Desyncs are caught by per-tick state hashes, and up to 8 players can join over UDP or Steam relay.
- **Deterministic physics**: Jolt Physics in its cross-platform deterministic build, plus a build-time checker (SimCheck) that rejects non-deterministic code in game simulation.
- **Fast on integrated graphics**: a DirectX 12 forward renderer with per-mesh instancing and automatic LODs runs at about 3,500 fps (2,000 boxes, 4 draw calls) on Intel Core Ultra 5 125U integrated graphics.
- **Tiny builds**: a shipped game is a single static exe of about 1.4 MB, with no redistributable needed.
- **Godot-style editor**: one general-purpose editor opens many game project folders. Game code is C++ and hot-reloads into the editor as a DLL, and one-click export makes a release folder.

Windows + DirectX 12 only for now. Minimum spec: Intel 11th-gen (Xe) integrated graphics.

Documentation: the separate `MealKit-docs` repository (English and Korean, published together with this repository) · [Roadmap](docs/ROADMAP.md) · [Decisions](docs/DECISIONS.md) · [Changelog](docs/CHANGELOG.md)

## License

[MIT](LICENSE). Third-party licenses: [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
