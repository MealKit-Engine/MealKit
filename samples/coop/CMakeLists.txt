# Reference sample of the engine repo: co-op third person (project folder = samples/coop: mealkit.project, assets/, art/).
#   Game build:   coop.exe (mp_test.ps1 -Exe build\Release\bin\coop.exe)
#   Editor build: coop_game.dll (loaded by MealKitEditor)
# Not a template (yet): it checks the stage 4 gameplay features as the core of a game (terrain, chasers on the nav grid,
# active ragdolls, virtual cameras and the intro cutscene, the animation state table).
#   sim/  deterministic simulation (SimCheck checks the rules at build time)
#   view/ camera, local input, drawing
#   app/  main (shipping exe), GameSession (runs one match), GameModule (editor entry point)
mealkit_add_game(coop SOURCE_DIR .)
