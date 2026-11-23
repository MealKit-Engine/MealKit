# Match flow (MealKit module)

Waiting for players, a start countdown, rounds and results for [MealKit](https://github.com/MealKit-Engine/MealKit) games. Docs: [docs/index.md](docs/index.md).

> **Beta (v0.x).** MealKit is under active development: APIs and file formats may change between versions, and projects are moved forward with the upgrade tool. Feedback and bug reports are welcome.

```
tools\module.ps1 add -Project <your project> matchflow
```

Tests (not copied into projects): `cmake -S tests -B build/tests -G Ninja -DMEALKIT_ROOT=<engine> -DMEALKIT_HEADLESS_ONLY=ON`, then build and run `matchflow_tests`.

MIT license.
