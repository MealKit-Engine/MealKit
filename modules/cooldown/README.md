# Cooldowns (MealKit module)

Tick-based cooldowns for [MealKit](https://github.com/MealKit-Engine/MealKit) games, with a fill bar `.ui` component. Docs: [docs/index.md](docs/index.md).

```
tools\module.ps1 add -Project <your project> https://github.com/MealKit-Engine/module-cooldown#v0.1.0
```

Tests (not copied into projects): `cmake -S tests -B build/tests -G Ninja -DMEALKIT_ROOT=<engine> -DMEALKIT_HEADLESS_ONLY=ON`, then build and run `cooldown_tests`.

MIT license.
