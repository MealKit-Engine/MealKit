# mkaudio: 소리 이벤트 런타임 (MealKit 오디오, 임시 이름. docs/DECISIONS.md 2026-10-26).
# 엔진 밖 라이브러리다: MealKit 헤더를 include하지 않는다(tools\check_libs.ps1). 오디오 2에서 이 폴더째 별도 저장소로 옮긴다.
# 쓰는 쪽이 정하는 값:
#   MKAUDIO_MINIAUDIO_DIR   miniaudio.h가 있는 폴더
#   MKAUDIO_STB_DIR         stb_vorbis.c가 있는 폴더
cmake_minimum_required(VERSION 3.25)

if(NOT MKAUDIO_MINIAUDIO_DIR OR NOT MKAUDIO_STB_DIR)
    message(FATAL_ERROR "mkaudio: set MKAUDIO_MINIAUDIO_DIR and MKAUDIO_STB_DIR")
endif()

add_library(mkaudio STATIC
    include/mkaudio/Mixer.h
    src/Mixer.cpp
    src/MiniaudioConfig.h
    src/MiniaudioImpl.cpp)
target_include_directories(mkaudio PUBLIC include)
target_include_directories(mkaudio SYSTEM PRIVATE ${MKAUDIO_MINIAUDIO_DIR} ${MKAUDIO_STB_DIR})
target_compile_features(mkaudio PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mkaudio PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    set_source_files_properties(src/MiniaudioImpl.cpp PROPERTIES COMPILE_OPTIONS "/W0") # 외부 라이브러리 구현
else()
    target_compile_options(mkaudio PRIVATE -Wall -Wextra)
    set_source_files_properties(src/MiniaudioImpl.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()
