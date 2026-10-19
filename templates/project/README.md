# @NAME@

[MealKit 엔진](@MEALKIT_ROOT_FWD@/README.md)으로 만드는 게임 프로젝트.
엔진과 에디터는 `@MEALKIT_ROOT@`에 있고, 이 폴더에는 **게임 코드, 에셋, 빌드 결과**만 있다.

## 폴더

| 폴더/파일 | 무엇 |
|---|---|
| `mealkit.project` | 프로젝트 설정: 이름, 엔진 위치(`engine`), 시작 씬(`scene`), 게임 항목(`game.*`). 에디터 [파일] > [프로젝트 설정]에서 고친다 |
| `source/` | 게임 코드. `sim/`(결정론 게임 규칙, 빌드 때 SimCheck 검사), `view/`(카메라·조작·그리기), `app/`(`GameSession` 한 판 진행, `main.cpp` 출시용 exe(`MEALKIT_GAME_MAIN` 한 줄, 나머지는 엔진), `GameModule.cpp` 에디터용 DLL 진입점) |
| `assets/` | **출시에 들어가는 것만.** 바로 밑에는 종류 폴더(`scenes/ models/ textures/ sounds/ effects/ ui/ fonts/ tables/ strings/ lut/`)와 정해진 파일(`game.tuning`, `view.tuning`, `input.bindings`, `feedback.table`, `sounds.table`)만 둔다. 종류 폴더 안의 하위 폴더는 자유 |
| `art/` | 외부 도구 원본(`.blend`, Penpot, PSD, 소리 프로젝트). 출시·콘텐츠 해시에 들어가지 않는다 |
| `modules/` | 소스 모듈 (4단계) |
| `third_party/` | 게임이 쓰는 외부 라이브러리. `<lib>/`마다 원본 `LICENSE`와 `mealkit.txt`(name, version, license, licenseFile, source, usedIn, kept). `sim/`에서는 쓸 수 없다 |
| `build/Editor/` | 에디터가 불러오는 게임 코드 `@NAME@_game.dll` |
| `build/Release/` | 출시용 `@NAME@.exe` (결정론 테스트, 직접 실행) |
| `export/@NAME@/` | **출시본**. 이 폴더째로 친구에게 주거나 Steam에 올린다 |

## 작업 흐름

1. `editor.bat`으로 에디터를 연다 (엔진의 `MealKitEditor.exe`가 이 프로젝트를 연다). 처음에는 게임 코드를 빌드하느라 잠깐 기다린다.
   엔진 에디터를 그냥 실행하면 **프로젝트 관리자**가 떠서 최근 프로젝트를 고를 수 있다.
2. VS Code로 이 폴더를 열고 **에디터를 켜 둔 채로** 게임 코드를 고친다.
3. `Ctrl+Shift+B`(게임 코드 빌드) → 에디터가 새 DLL을 감지해 **자동으로 바꿔 끼운다**(핫 리로드). 에디터에서 `Ctrl+R`을 눌러도 된다.
   씬·카메라·선택은 그대로 남는다. 플레이 중이면 정지한 뒤에 바뀐다.
4. 씬 편집과 `F5` 플레이는 에디터에서. 에디터 플레이 중에는 같은 게임 코드가 DLL로 돈다.
5. **출시 빌드**: 에디터 `빌드 → 출시 빌드` (`Ctrl+B`) 또는 `build.bat Export` → `export\@NAME@\`.
6. 멀티플레이 확인: `Tasks: Run Task` → `Multiplayer Determinism Test` (PASS가 나와야 한다).
7. 디버깅: VS Code `F5` → `Editor (debug game code)`로 에디터 안의 게임 코드에 중단점을 건다.

## 규칙

- `source/sim/`은 락스텝 결정론 코드다. 엔진 `docs/CODING_MODEL.md`의 "결정론 규칙"을 따른다 (CRT 삼각함수 금지, `Pcg32`만, 순회 순서 고정, 새 상태는 `hash()`에).
- 멀티플레이는 모든 플레이어가 **같은 exe, 같은 씬, 같은 모델 파일**이어야 접속된다. 출시 폴더째로 나눠 준다.
- Steam 앱 ID는 `CMakeLists.txt`의 `STEAM_APP_ID` (기본 480 = Valve 테스트 앱). 내 앱 ID로 바꾸면 출시 폴더에 `steam_appid.txt`가 들어가지 않는다(Steam이 실행해 준다).
- 엔진을 고치는 일은 엔진 폴더(`@MEALKIT_ROOT@`)에서 한다. 엔진이 바뀌면 에디터가 게임 코드를 다시 빌드한다.
- 폴더 구조는 엔진이 정한다. `@MEALKIT_ROOT@\tools\check_layout.ps1 -Project .`이 잘못 놓인 파일을 찾고(Export도 같은 검사로 실패한다), 에디터는 프로젝트를 열 때 경고한다.
