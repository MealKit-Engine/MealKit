# 출시 폴더 만들기 (game_export 타깃이 cmake -P로 실행한다).
# 입력: GAME_EXE, ASSETS_DIR, OUT_DIR, STEAM_DLL(없으면 빈 값), STEAM_APP_ID, NOTICES
foreach(var GAME_EXE ASSETS_DIR OUT_DIR NOTICES)
    if(NOT ${var})
        message(FATAL_ERROR "MealKitExport: ${var} is not set")
    endif()
endforeach()

# 지난 출시본을 지우고 새로 만든다 (지워진 에셋이 남지 않도록)
file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")

file(COPY "${GAME_EXE}" DESTINATION "${OUT_DIR}")
if(EXISTS "${ASSETS_DIR}")
    file(COPY "${ASSETS_DIR}" DESTINATION "${OUT_DIR}"
        PATTERN "*.autosave" EXCLUDE)
else()
    message(WARNING "MealKitExport: no assets folder at ${ASSETS_DIR}")
endif()
file(COPY "${NOTICES}" DESTINATION "${OUT_DIR}")

if(STEAM_DLL)
    file(COPY "${STEAM_DLL}" DESTINATION "${OUT_DIR}")
    # 480(테스트 앱)이면 Steam 없이 실행해도 SteamAPI_Init이 되도록 넣는다. 내 앱 ID라면 Steam이 실행하므로 넣지 않는다.
    if(STEAM_APP_ID STREQUAL "480")
        file(WRITE "${OUT_DIR}/steam_appid.txt" "480")
    endif()
endif()

get_filename_component(exe_name "${GAME_EXE}" NAME)
message(STATUS "Exported ${exe_name} to ${OUT_DIR}")
