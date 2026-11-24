# Reference samples

The reference samples (`sandbox`: physics party versus, also the `arena` template; `coop`: co-op third person) are projects in their own repos, `MealKit-Engine/sample-sandbox` and `sample-coop`. The engine repo pins one commit of each in `samples.lock` and fetches them into the git-ignored `samples/<name>`. Decided 2026-11-24 (`docs/DECISIONS.md` "The reference samples leave the engine repo"); implementation in `docs/CHANGELOG.md` ("The sample repos", "Build and checks fetch the locked samples", "Templates come from the lock").

## Lock and tool

`samples.lock`: `@format sample-lock 1` and lines `sample <name> <repo url> <tag> <commit>`. `tools\sample.ps1` (it shares its repo and lock helpers with `module_engine.ps1` in `tools/lock_repo.ps1`):

| Command | What it does |
|---|---|
| `status` | Each folder against the lock: `ok`, `LOCAL` (linked to a checkout), `MISSING`, or `DIFF` (changed, missing or extra files; `build/`, `export/`, `*.autosave` and `CLAUDE.md` don't count). Exit 1 for `MISSING` and `DIFF` |
| `sync [name ...]` | Rewrite `samples/<name>` from the locked commit (cloned into the per-user cache `MealKit/samples`, or read from the sibling checkout `..\sample-<name>`) |
| `bump <name> [tag]` | Lock the checkout's newest `v*` tag (or the given one) and sync it |
| `local on\|off [name ...]` | `on`: `samples/<name>` becomes a junction (symbolic link on Linux and macOS) to the sibling `..\sample-<name>`; skipped when the folder has changes the lock doesn't have. `off`: remove the link and sync |

## Where the lock is used

- **Build**: the root `CMakeLists.txt` adds a sample only when its folder is there; building the engine repo runs `sample.ps1 sync` for missing ones. CI syncs after checkout.
- **Checks**: `check.ps1`, `ci_test.ps1`, `mp_test.ps1`, `record_replay.ps1` (and `live_probe.ps1` for its default project) stop at once when `sample.ps1 status` fails (`tools/assert_samples.ps1`). A change of `samples.lock` runs the groups a change under `samples/` runs.
- **Unit tests** don't read samples: `engine/tests/data/` holds the few files they need, and `mealkit_tests` passes with no `samples/` folder.
- **Templates**: `new_project.ps1 -Template arena|coop` and `upgrade_project.ps1 -Source` read the samples through `sample.ps1`. `-Template` also takes a git address or folder of another sample repo. `.mealkit-template` records `@sample <name> <url> <tag> <commit>`. The Manager's New Project cards come from `samples.lock` and `templates/previews/`.

## Changing a sample together with the engine

1. `tools\sample.ps1 local on <name>` (links `..\sample-<name>`).
2. Change the engine and the sample; build and check.
3. Commit in the sample repo and tag it (`v0.2.0`); `tools\sample.ps1 local off <name>`.
4. `tools\sample.ps1 bump <name>` and commit `samples.lock` in the engine, in the same change as the engine edit.

Samples' replays (`replays/*.rec`) and scenarios belong to the sample repo; re-record them there (`tools\record_replay.ps1`) and bump the lock. Cost accepted in the decision: each engine change that touches a sample is a sample commit and tag plus a lock bump.
