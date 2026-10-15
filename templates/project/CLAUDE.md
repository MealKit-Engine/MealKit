# @NAME@

MealKit 엔진(`@MEALKIT_ROOT@`, DirectX 12 전용 3D 멀티플레이, C++20/MSVC/CMake+Ninja)으로 만드는 게임 프로젝트.
사용자와는 한국어로 대화한다. 코드 주석과 에디터 UI는 한국어, 로그 메시지와 씬 파일 키는 영어로 쓴다.

## 명령

- 게임 코드 빌드(에디터용 DLL, 핫 리로드): `build.bat Editor` → `build\Editor\bin\@NAME@_game.dll`
- 출시용 빌드: `build.bat` (Release) / `build.bat Debug` / `build.bat Profile` / `build.bat Export` (→ `export\@NAME@\`)
- 실행: `build\Release\bin\@NAME@.exe --help` (게임), `editor.bat` (에디터)
- 결정론 테스트: `powershell -ExecutionPolicy Bypass -File @MEALKIT_ROOT@\tools\mp_test.ps1 -Clients 3 -Ticks 900 -Exe build\Release\bin\@NAME@.exe` → PASS 필수
- 게임 화면 확인: `build\Release\bin\@NAME@.exe --bot --exit-tick 300 --capture out.png 240`
- 에디터 화면 확인: `@MEALKIT_ROOT@\build\Editor\bin\MealKitEditor.exe --project . --frames 60 --select 5 --capture out.png` (플레이 모드는 `--play --bot` 추가)

## 규칙

- 엔진 코드와 에디터는 이 폴더에 없다. 엔진을 고쳐야 하면 `@MEALKIT_ROOT@`에서 고친다 (그쪽 CLAUDE.md를 따른다).
- `source/World.*`는 결정론 코드다: CRT 삼각함수 금지, `/fp:precise` 유지, `Pcg32`만 사용, 순회·바디 생성 순서 고정, 새 상태는 `hash()`에 추가.
  시뮬레이션 코드를 바꾸면 결정론 테스트로 디싱크가 없는지 확인한다.
- 소스 파일을 추가하면 `CMakeLists.txt`의 `mealkit_add_game(... SOURCES ...)`에 넣는다.
- 게임 로직은 `source/World.*`, 한 판 진행은 `source/GameSession.*` (`mealkit::game::GameInstance` 구현, 출시 exe와 에디터 DLL이 공유).
  `source/GameModule.cpp`의 `MEALKIT_GAME_MODULE`이 에디터가 부르는 진입점이다.
- 씬 충돌 데이터나 모델 파일이 바뀌면 콘텐츠 해시가 바뀌어, 다른 데이터를 쓰는 피어는 접속이 거부된다.
