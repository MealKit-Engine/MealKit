# Reference sample of the engine repo (project folder = samples/sandbox: mealkit.project, assets/, art/, modules/).
#   Game build:   sandbox.exe (shipping; used by the determinism test mp_test.ps1)
#   Editor build: sandbox_game.dll (loaded by MealKitEditor)
# Also the template for new game projects (tools/new_project.ps1 copies sim/ view/ app/ into the project's source/).
#   sim/  deterministic simulation (SimCheck checks the rules at build time)
#   view/ camera, local input, drawing
#   app/  main (shipping exe), GameSession (runs one match), GameModule (editor entry point)
mealkit_add_game(sandbox SOURCE_DIR .)
