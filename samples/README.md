# Reference samples

The reference samples (`sandbox`, `coop`) are moving to their own repos (`MealKit-Engine/sample-<name>`), pinned by `samples.lock` and fetched into `samples/<name>` (git-ignored) by `tools\sample.ps1`. Until they have moved (`docs/ROADMAP.md` "Samples out of the engine repo"), the folders here are still part of the engine repo.

```
pwsh -NoProfile -File tools/sample.ps1 status               # folders vs samples.lock (exit 1 when one is missing or differs)
pwsh -NoProfile -File tools/sample.ps1 sync [name ...]      # fetch the locked commit into samples/<name>
pwsh -NoProfile -File tools/sample.ps1 bump <name> [tag]    # lock a newer tag and sync it
pwsh -NoProfile -File tools/sample.ps1 local on|off [name ...]  # link a sibling ../sample-<name> checkout while changing it with the engine
```

Changing a sample together with the engine: `sample.ps1 local on <name>`, edit and commit in the sample repo, tag it, `sample.ps1 local off`, `sample.ps1 bump <name>`, and commit `samples.lock` with the engine change.
