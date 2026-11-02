# mkui: game UI runtime (.ui reading, layout, text, view model binding → quads). No engine headers: the engine's mealkit/ui/ is the glue
# (files, strings, image decoding, atlas upload, Reflect view models, drawing). Moves to its own repo later (ROADMAP "UI 2").
# Values the including side sets:
#   MKUI_STB_DIR          folder containing stb_truetype.h (the implementation is compiled here)
#   MKUI_MKCONTRACT_DIR   folder containing mkcontract/Text.h (the mkcontract target must exist: "@format ui N" lines)
cmake_minimum_required(VERSION 3.25)
project(mkui VERSION 0.1.0 LANGUAGES CXX)

add_library(mkui STATIC
    include/mkui/Types.h
    include/mkui/Host.h
    include/mkui/Document.h
    include/mkui/Atlas.h
    include/mkui/Text.h
    include/mkui/Ui.h
    src/Document.cpp
    src/Atlas.cpp
    src/Text.cpp
    src/Ui.cpp
    src/StbTruetype.cpp)
target_include_directories(mkui PUBLIC include)
target_include_directories(mkui SYSTEM PRIVATE ${MKUI_STB_DIR})
target_link_libraries(mkui PRIVATE mkcontract)
target_compile_features(mkui PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mkui PRIVATE /permissive- /utf-8 /Zc:__cplusplus)
    set_source_files_properties(src/Document.cpp src/Atlas.cpp src/Text.cpp src/Ui.cpp PROPERTIES COMPILE_OPTIONS "/W4")
    set_source_files_properties(src/StbTruetype.cpp PROPERTIES COMPILE_OPTIONS "/W0") # external library implementation
else()
    target_compile_options(mkui PRIVATE -Wall -Wextra)
    set_source_files_properties(src/StbTruetype.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()
