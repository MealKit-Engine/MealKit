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
- `source/sim/`은 결정론 코드다: CRT 삼각함수 금지(`mealkit/core/DetMath.h`를 쓴다), `/fp:precise` 유지, `Pcg32`만 사용, 순회·바디 생성 순서 고정.
  틱을 넘어 남는 상태는 `WorldState` 안에 `MEALKIT_FIELD`로 선언한다. 선언하면 해시·스냅샷·에디터 [상태] 패널에 저절로 들어간다.
  빌드 때 SimCheck가 검사해 어기면 `error SIMnnn`으로 빌드가 멈춘다(대신 쓸 것을 알려 준다). 규칙의 근거는 엔진의 `docs/CODING_MODEL.md`.
  시뮬레이션 코드를 바꾸면 결정론 테스트로 디싱크가 없는지 확인한다.
- 게임 코드 폴더: `source/sim/`(규칙: 시스템별 파일, 시스템 함수는 `World&`만 받고 `World::step()`이 순서대로 호출),
  `source/view/`(카메라·로컬 입력·그리기, sim 상태는 읽기만), `source/app/`(`GameSession` = `mealkit::game::GameInstance` 구현, `main.cpp`, `GameModule.cpp`의 `MEALKIT_GAME_MODULE`).
  `sim/`에서 매치 중 생기고 사라지는 객체는 `mealkit::sim::Pool<T>`, "N틱 뒤"는 `TickTimer`, 화면에 알릴 일은 `world.events`(`TickEvents`)로 한다.
  화면 쪽은 엔진 서비스를 `mealkit::game::GameContext&`로 받는다 (`docs/CODING_MODEL.md`).
- 사람(`PlayerSlot`)과 몸(`Pawn`)은 나뉘고 `Controller`가 잇는다(플레이어 또는 AI). 리스폰·관전·탑승·AI는 Pawn을 새로 만들거나 `possess`/`unpossess`로 바꿔 구현한다 (`source/sim/Pawns.cpp`).
- `GameSession`은 엔진의 `game::Simulation<World>`로 틱을 돌린다(그림자 시뮬레이션, 일시정지·되감기가 여기서 나온다). 틱 루프를 직접 짜지 않는다.
- 에디터에 놓을 새 물체 종류는 `source/sim/Objects.h`에 `MEALKIT_OBJECT(이름) { MEALKIT_FIELD(타입, 필드, 기본값, "표시 이름"); };`로 선언하고,
  `World.cpp`의 `loadScene`에서 `scene::readObject<이름>(entity)`로 읽는다. 게임 코드를 다시 빌드하면 에디터 [추가] 메뉴에 나온다.
  include는 `"sim/World.h"`처럼 `source/` 기준으로 쓴다. 파일을 추가해도 `CMakeLists.txt`를 고치지 않는다.
- **숫자는 코드 상수로 두지 않는다** (엔진 `docs/CODING_MODEL.md` "숫자는 어디에 두나"): 게임 규칙 값 → `Tuning`, 맵마다 다른 값(범위, 개수, 높이) → 씬 물체(영역은 `kObjectFlags = mealkit::reflect::kObjectArea`인 `MEALKIT_OBJECT`의 위치·크기),
  보이기만 하는 값 → `ViewTuning`. `sim/`의 실수 리터럴은 SimCheck가 경고(SIM012)한다. 단위 변환·알고리즘 상수만 줄 끝 `// const-ok: <이유>`로 남긴다.
- 튜닝 값(속도, 쿨다운 같은 숫자)은 코드 상수 대신 `source/sim/Tuning.h`의 `MEALKIT_FIELD`로 선언하고 `world.tuning`으로 읽는다. 값은 `assets/game.tuning`, 에디터 [튜닝] 패널에서 고친다.
  보이기만 하는 값(카메라, 색, 효과 시간)은 `source/view/ViewTuning.h`(`assets/view.tuning`, [튜닝] 패널 "화면" 탭)에 두고 화면 쪽에서 `ctx.viewTuning<ViewTuning>()`으로 읽는다. 콘텐츠 해시에 들어가지 않아 여러 명 플레이 중에도 바로 바뀐다.
  시간 값은 `MEALKIT_FIELD(mealkit::Ticks, 이름, mealkit::Ticks::seconds(0.5f), "설명 (초)")`로 선언한다(파일·에디터는 초, `sim/`은 정수 틱).
- 엔진을 올린 뒤 에디터가 "프로젝트 업그레이드"를 물으면 받아들인다(또는 `@MEALKIT_ROOT@\tools\upgrade_project.ps1 -Project .`).
  빌드 파일만 새로 바꾸고 직접 고친 파일은 `<파일>.new`로 남긴다. `.mealkit-template`은 지우지 않는다.
- 폴더 구조는 엔진이 정한다: `assets/` 바로 밑은 종류 폴더(`scenes models textures sounds effects ui fonts tables strings lut`)와 정해진 파일만, 외부 도구 원본(`.blend`, `.psd`, Penpot)은 `art/`.
  외부 라이브러리는 `third_party/<lib>/`에 원본 `LICENSE`와 `mealkit.txt`(name, version, license, source, usedIn, 빌드할 게 있으면 `cmakeTarget`)를 두면 게임 exe·DLL에 붙는다(`sim/`에서는 SIM011).
  `@MEALKIT_ROOT@\tools\check_layout.ps1 -Project .`이 검사하고, Export도 같은 검사로 실패한다.
- 씬 충돌 데이터, 모델 파일, 튜닝 값이 바뀌면 콘텐츠 해시가 바뀌어, 다른 데이터를 쓰는 피어는 접속이 거부된다.
