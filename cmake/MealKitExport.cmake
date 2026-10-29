# Build the shipping folder (run by the game_export target via cmake -P).
# Input: GAME_EXE, ASSETS_DIR, PROJECT_FILE, OUT_DIR, STEAM_DLL/EOS_DLL (empty if absent), STEAM_APP_ID, NOTICES (merged notices), EXE_BUDGET_MB
foreach(var GAME_EXE ASSETS_DIR PROJECT_FILE OUT_DIR NOTICES)
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
if(EXISTS "${ASSETS_DIR}")
    file(COPY "${ASSETS_DIR}" DESTINATION "${OUT_DIR}"
        PATTERN "*.autosave" EXCLUDE
        PATTERN ".gitkeep" EXCLUDE)
else()
    message(WARNING "MealKitExport: no assets folder at ${ASSETS_DIR}")
endif()
get_filename_component(project_dir "${PROJECT_FILE}" DIRECTORY)
# Source module assets (docs/MODULES.md): modules/<name>/assets/ → shipping folder modules/<name>/assets/ (asset path "modules/<name>/...")
file(GLOB module_asset_dirs LIST_DIRECTORIES true "${project_dir}/modules/*/assets")
foreach(dir IN LISTS module_asset_dirs)
    get_filename_component(module_dir "${dir}" DIRECTORY)
    get_filename_component(module_name "${module_dir}" NAME)
    file(COPY "${dir}" DESTINATION "${OUT_DIR}/modules/${module_name}"
        PATTERN "*.autosave" EXCLUDE
        PATTERN ".gitkeep" EXCLUDE)
endforeach()
# Model texture cache (asset/TextureCache.h): shipping builds don't compress, they only read BC files in cache/textures next to the exe.
# The cache is made when the editor or a dev build loads a model (if missing, that texture shows white and is logged)
# Same for the sky HDRI cache (asset/Environment.h): shipping builds don't bake, they only read cache/environments next to the exe
file(REMOVE_RECURSE "${OUT_DIR}/cache")
foreach(cache textures environments)
    if(EXISTS "${project_dir}/build/cache/${cache}")
        file(COPY "${project_dir}/build/cache/${cache}" DESTINATION "${OUT_DIR}/cache")
    endif()
endforeach()
file(COPY "${NOTICES}" DESTINATION "${OUT_DIR}")

# Project settings (the game reads the start scene etc.). The engine/engineVersion lines containing dev PC paths are removed.
if(EXISTS "${PROJECT_FILE}")
    file(STRINGS "${PROJECT_FILE}" project_lines ENCODING UTF-8)
    set(project_text "")
    foreach(line IN LISTS project_lines)
        if(NOT line MATCHES "^[ \t]*(engine|engineVersion)([ \t=]|$)")
            string(APPEND project_text "${line}\n")
        endif()
    endforeach()
    file(WRITE "${OUT_DIR}/mealkit.project" "${project_text}")
else()
    message(WARNING "MealKitExport: no project file at ${PROJECT_FILE} - the game uses the default project settings")
endif()

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
