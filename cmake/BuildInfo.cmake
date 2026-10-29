# Generate the build info header (mealkit_add_game calls this every build): cmake -DOUT=... -DLIST=... -DPROJECT_FILE=... -DPROJECT_DIR=... -DENGINE_DIR=... -P BuildInfo.cmake
#   MEALKIT_BUILD_GAME_VERSION  version from mealkit.project
#   MEALKIT_BUILD_COMMIT        git commit of the project folder (short hash, + at the end if files are modified)
#   MEALKIT_BUILD_SIM_VERSION   simulation version: game version + hash of the sources that touch the simulation (files in LIST).
#                               Line endings are normalized to LF and paths hashed relative, so the same source gives the same value across OSes/compilers (crossplay).
# If the content is the same, the file is not rewritten (to avoid recompiling).
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

set(text "// Generated file (cmake/BuildInfo.cmake, every build). Don't edit.\n#pragma once\n#define MEALKIT_BUILD_GAME_VERSION \"${version}\"\n#define MEALKIT_BUILD_COMMIT \"${commit}\"\n#define MEALKIT_BUILD_SIM_VERSION 0x${sim}ull\n")
set(old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUT}" "${text}")
endif()
