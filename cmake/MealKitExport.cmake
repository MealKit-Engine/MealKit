# 출시 폴더 만들기 (game_export 타깃이 cmake -P로 실행한다).
# 입력: GAME_EXE, ASSETS_DIR, PROJECT_FILE, OUT_DIR, STEAM_DLL·EOS_DLL(없으면 빈 값), STEAM_APP_ID, NOTICES(합친 고지문), EXE_BUDGET_MB
foreach(var GAME_EXE ASSETS_DIR PROJECT_FILE OUT_DIR NOTICES)
    if(NOT ${var})
        message(FATAL_ERROR "MealKitExport: ${var} is not set")
    endif()
endforeach()

# 출시 exe 예산 (ROADMAP "성능 예산": 모든 플랫폼 5 MB, 에셋 제외)
if(EXE_BUDGET_MB)
    file(SIZE "${GAME_EXE}" exe_bytes)
    math(EXPR exe_budget "${EXE_BUDGET_MB} * 1024 * 1024")
    math(EXPR exe_kb "${exe_bytes} / 1024")
    if(exe_bytes GREATER exe_budget)
        message(FATAL_ERROR "MealKitExport: ${GAME_EXE} is ${exe_kb} KB, over the ${EXE_BUDGET_MB} MB budget (see docs/ROADMAP.md \"Performance budget\")")
    endif()
    message(STATUS "Game executable: ${exe_kb} KB (budget ${EXE_BUDGET_MB} MB)")
endif()

# 지난 출시본을 지우고 새로 만든다 (지워진 에셋이 남지 않도록)
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
# 모델 텍스처 캐시 (asset/TextureCache.h): 출시 빌드는 압축하지 않고 exe 옆 cache/textures의 BC 파일만 읽는다.
# 캐시는 에디터나 개발 빌드가 모델을 불러올 때 만든다 (없으면 그 텍스처는 흰색으로 나오고 로그에 남는다)
# 하늘 HDRI 캐시 (asset/Environment.h)도 같다: 출시 빌드는 굽지 않고 exe 옆 cache/environments만 읽는다
get_filename_component(project_dir "${PROJECT_FILE}" DIRECTORY)
file(REMOVE_RECURSE "${OUT_DIR}/cache")
foreach(cache textures environments)
    if(EXISTS "${project_dir}/build/cache/${cache}")
        file(COPY "${project_dir}/build/cache/${cache}" DESTINATION "${OUT_DIR}/cache")
    endif()
endforeach()
file(COPY "${NOTICES}" DESTINATION "${OUT_DIR}")

# 프로젝트 설정 (게임이 시작 씬 등을 읽는다). 개발 PC 경로가 든 engine·engineVersion 줄은 뺀다.
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
    # 480(테스트 앱)이면 Steam 없이 실행해도 SteamAPI_Init이 되도록 넣는다. 내 앱 ID라면 Steam이 실행하므로 넣지 않는다.
    if(STEAM_APP_ID STREQUAL "480")
        file(WRITE "${OUT_DIR}/steam_appid.txt" "480")
    endif()
endif()

get_filename_component(exe_name "${GAME_EXE}" NAME)
message(STATUS "Exported ${exe_name} to ${OUT_DIR}")
