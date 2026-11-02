# Build the shipping folder (run by the game_export target via cmake -P).
# Input: GAME_EXE, PACK (cooked by <game>.exe --cook, game/Cook.h), PROJECT_FILE, ENGINE_VERSION, OUT_DIR, STEAM_DLL/EOS_DLL (empty if absent), STEAM_APP_ID, NOTICES (merged notices), EXE_BUDGET_MB
foreach(var GAME_EXE PACK PROJECT_FILE OUT_DIR NOTICES)
    if(NOT ${var})
        message(FATAL_ERROR "MealKitExport: ${var} is not set")
    endif()
endforeach()

# Shipping exe budget (ROADMAP "Performance budget": 5 MB on all platforms, excluding assets)
if(EXE_BUDGET_MB)
    file(SIZE "${GAME_EXE}" exe_bytes)
    math(EXPR exe_budget "${EXE_BUDGET_MB} * 1024 * 1024")
    math(EXPR exe_kb "${exe_bytes} / 1024")
    if(exe_bytes GREATER exe_budget)
        message(FATAL_ERROR "MealKitExport: ${GAME_EXE} is ${exe_kb} KB, over the ${EXE_BUDGET_MB} MB budget (see docs/ROADMAP.md \"Performance budget\")")
    endif()
    message(STATUS "Game executable: ${exe_kb} KB (budget ${EXE_BUDGET_MB} MB)")
endif()

# Delete the previous release and build fresh (so deleted assets don't linger)
file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")

file(COPY "${GAME_EXE}" DESTINATION "${OUT_DIR}")
# Game data: one pack (platform/Pack.h) holding the cooked assets the game reaches and mealkit.project. No assets folder or caches
if(NOT EXISTS "${PACK}")
    message(FATAL_ERROR "MealKitExport: no pack at ${PACK} (the cook step failed, see above)")
endif()
get_filename_component(exe_stem "${GAME_EXE}" NAME_WE)
file(COPY_FILE "${PACK}" "${OUT_DIR}/${exe_stem}.pak")
file(COPY "${NOTICES}" DESTINATION "${OUT_DIR}")

# Build stamp: engine version, git commit of the project, content hash of the pack. --help and bug reports show it
get_filename_component(project_dir "${PROJECT_FILE}" DIRECTORY)
set(content "unknown")
if(EXISTS "${PACK}.txt")
    file(STRINGS "${PACK}.txt" content_line REGEX "^content " ENCODING UTF-8)
    string(REGEX REPLACE "^content " "" content "${content_line}")
endif()
execute_process(COMMAND git -C "${project_dir}" rev-parse --short HEAD OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE git_result)
if(NOT git_result EQUAL 0 OR NOT commit)
    set(commit "unknown")
else()
    execute_process(COMMAND git -C "${project_dir}" status --porcelain OUTPUT_VARIABLE dirty ERROR_QUIET)
    if(dirty)
        string(APPEND commit "+")
    endif()
endif()
string(TIMESTAMP built "%Y-%m-%d %H:%M")
file(WRITE "${OUT_DIR}/build.txt" "engine ${ENGINE_VERSION}
commit ${commit}
content ${content}
built ${built}
")

if(EOS_DLL)
    file(COPY "${EOS_DLL}" DESTINATION "${OUT_DIR}")
endif()
if(STEAM_DLL)
    file(COPY "${STEAM_DLL}" DESTINATION "${OUT_DIR}")
    # For 480 (test app), included so SteamAPI_Init works without launching via Steam. For your own app ID, Steam launches it, so it's not included.
    if(STEAM_APP_ID STREQUAL "480")
        file(WRITE "${OUT_DIR}/steam_appid.txt" "480")
    endif()
endif()

get_filename_component(exe_name "${GAME_EXE}" NAME)
message(STATUS "Exported ${exe_name} to ${OUT_DIR}")
