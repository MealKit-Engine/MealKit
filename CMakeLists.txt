cmake_minimum_required(VERSION 3.25)
project(MealKit LANGUAGES CXX)

# Engine repository build: engine + sample game (samples/sandbox, for determinism tests).
# Real games are made in a separate project folder (tools\new_project.ps1, docs repo "Manual > Engine folder and game projects").
include(cmake/MealKit.cmake)

add_subdirectory(samples/sandbox)
