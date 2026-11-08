# Reference sample of the engine repo (project folder = samples/sandbox: mealkit.project, assets/, art/, modules/).
#   Game build:   sandbox.exe (shipping; used by the determinism test mp_test.ps1)
#   Editor build: sandbox_game.dll (loaded by MealKitEditor)
# Also the template for new game projects (tools/new_project.ps1 copies sim/ view/ app/ into the project's source/).
#   sim/  deterministic simulation (SimCheck checks the rules at build time)
#   view/ camera, local input, drawing
#   app/  main (shipping exe), GameSession (runs one match), GameModule (editor entry point)
mealkit_add_game(sandbox SOURCE_DIR .)
# Preset check (build.bat Presets): the same game built without the net module, plays solo only (docs/ENGINE_MODULES.md "Presets")
if("solo" IN_LIST MEALKIT_EXTRA_PRESETS)
    mealkit_add_game(sandbox_solo SOURCE_DIR . PRESET solo)
endif()
if("standard/audio" IN_LIST MEALKIT_EXTRA_PRESETS)
    mealkit_add_game(sandbox_noaudio SOURCE_DIR . WITHOUT audio) # without the audio module (app/GameSession.cpp keeps its sound calls in #if MEALKIT_HAS_AUDIO)
endif()
