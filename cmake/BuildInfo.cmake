# 빌드 정보 헤더 만들기 (mealkit_add_game이 빌드마다 부른다): cmake -DOUT=... -DLIST=... -DPROJECT_FILE=... -DPROJECT_DIR=... -DENGINE_DIR=... -P BuildInfo.cmake
#   MEALKIT_BUILD_GAME_VERSION  mealkit.project의 version
#   MEALKIT_BUILD_COMMIT        프로젝트 폴더의 git 커밋 (짧은 해시, 고친 파일이 있으면 끝에 +)
#   MEALKIT_BUILD_SIM_VERSION   시뮬레이션 버전: 게임 버전 + 시뮬레이션에 닿는 소스(LIST의 파일)의 해시.
#                               줄바꿈을 LF로 맞추고 상대 경로로 해시하므로 OS·컴파일러가 달라도 같은 소스면 같은 값이다(크로스플레이).
# 내용이 같으면 파일을 다시 쓰지 않는다 (다시 컴파일하지 않게).
file(STRINGS "${LIST}" files ENCODING UTF-8)

set(version "0.1.0")
if(EXISTS "${PROJECT_FILE}")
    file(STRINGS "${PROJECT_FILE}" version_lines REGEX "^[ \t]*version([ \t=]|$)" ENCODING UTF-8)
    foreach(line IN LISTS version_lines)
        string(REGEX REPLACE "^[ \t]*version[ \t]*=?[ \t]*" "" line "${line}")
        string(STRIP "${line}" line)
        if(line)
            set(version "${line}")
        endif()
    endforeach()
endif()

set(digest "version ${version}\n")
foreach(f IN LISTS files)
    if(NOT EXISTS "${f}")
        continue()
    endif()
    file(READ "${f}" content)
    string(REPLACE "\r" "" content "${content}")
    string(SHA1 h "${content}")
    set(rel "${f}")
    string(REPLACE "${PROJECT_DIR}/" "project/" rel "${rel}")
    string(REPLACE "${ENGINE_DIR}/" "engine/" rel "${rel}")
    string(APPEND digest "${rel} ${h}\n")
endforeach()
string(SHA1 sim "${digest}")
string(SUBSTRING "${sim}" 0 16 sim)

set(commit "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD WORKING_DIRECTORY "${PROJECT_DIR}"
        OUTPUT_VARIABLE git_out RESULT_VARIABLE git_result OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(git_result EQUAL 0 AND git_out)
        set(commit "${git_out}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no WORKING_DIRECTORY "${PROJECT_DIR}"
            OUTPUT_VARIABLE git_dirty ERROR_QUIET)
        if(git_dirty)
            string(APPEND commit "+")
        endif()
    endif()
endif()

set(text "// 만든 파일 (cmake/BuildInfo.cmake, 빌드마다). 고치지 않는다.\n#pragma once\n#define MEALKIT_BUILD_GAME_VERSION \"${version}\"\n#define MEALKIT_BUILD_COMMIT \"${commit}\"\n#define MEALKIT_BUILD_SIM_VERSION 0x${sim}ull\n")
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUT}" "${text}")
endif()
