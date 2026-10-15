cmake_minimum_required(VERSION 3.25)
project(MealKit LANGUAGES CXX)

# 엔진 저장소 빌드: 엔진 + 예제 게임(samples/sandbox, 결정론 테스트용).
# 실제 게임은 별도 프로젝트 폴더에서 만든다 (tools\new_project.ps1, README "게임 프로젝트").
include(cmake/MealKit.cmake)

add_subdirectory(samples/sandbox)
