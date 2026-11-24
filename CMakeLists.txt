cmake_minimum_required(VERSION 3.25)
project(MealKit LANGUAGES CXX)

# Engine repository build: engine + reference samples (samples/<name>, fetched from their own repos by tools/sample.ps1; each is a project with mealkit.project and assets/).
# Real games are made in a separate project folder (tools\new_project.ps1, docs repo "Manual > Engine folder and game projects").
include(cmake/MealKit.cmake)

# Samples are fetched by tools/sample.ps1 (samples.lock): only the folders present are built
foreach(mealkit_sample sandbox coop)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/${mealkit_sample}/CMakeLists.txt")
        add_subdirectory(samples/${mealkit_sample})
    else()
        message(STATUS "Sample '${mealkit_sample}' is not fetched: run tools/sample.ps1 sync")
    endif()
endforeach()
