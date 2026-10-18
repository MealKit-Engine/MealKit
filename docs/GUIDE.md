# MealKit 가이드

> 예전 README 전체를 그대로 옮긴 임시 문서다. 나중에 별도 문서 프로젝트(고도의 godot-docs처럼)로 옮겨 다시 쓴다.

사무용 PC(Intel 내장 그래픽)에서도 가볍고 빠르게 돌아가는 **3D 멀티플레이 게임 엔진**. 지금은 Windows + DirectX 12, 다른 플랫폼은 [로드맵](ROADMAP.md) 5단계.

## 원칙

- **반드시 지킬 것**: 3D, 멀티플레이.
- 게임보다 엔진이 가볍고 빠른 것이 우선. 엔진 위의 게임은 바뀔 수 있다.
- 여러 플랫폼을 지원하되 DX12가 기준이다. OS API는 플랫폼 계층(`mealkit/platform/`), 그래픽 API는 엔진이 만드는 얇은 RHI(`mealkit/rhi/`) 뒤에서만 쓰고, 다른 API에 맞추려고 기능을 낮추지 않는다.
- 목표: PC·모바일·웹·콘솔 출시, Windows·macOS·Linux 에디터, 모든 플랫폼 사이 크로스플레이(로드맵 5단계). 시뮬레이션 코드에는 플랫폼 분기를 두지 않는다. 2D 게임은 지원하지 않는다.
- 이 저장소에서는 게임을 만들지 않는다. 엔진 기능은 `samples/`의 기준 샘플에서 실제로 써서 검증한다.
- 엔진은 결정론 시뮬레이션, 락스텝 네트워크, 렌더러, 에디터만 만들고 나머지는 완성된 외부 것을 가져다 쓴다. 제작 도구(Blender, Penpot, Effekseer, VS Code)는 자유롭게 쓰고,
  게임에 들어가는 라이브러리는 버전을 고정해 `third_party/`에 넣는다(아래 표). 시뮬레이션에 닿는 라이브러리는 결정론을 검증한 것만 쓴다.
- 한 문제에 답은 하나. 전역 접근, 오브젝트 구성, 풀링, 비동기 같은 설계 질문의 답은 [docs/CODING_MODEL.md](CODING_MODEL.md)에 정해 두고 그것만 쓴다.

## 엔진 폴더와 게임 프로젝트 폴더

고도처럼 **범용 에디터 하나**가 여러 **게임 프로젝트 폴더**를 연다. 게임 코드는 C++이고, 에디터는 프로젝트의 게임 코드를 DLL로 불러온다.

| 폴더 | 무엇 | 여기서 하는 일 |
|---|---|---|
| `D:\MealKit` (이 저장소) | 엔진(`engine/`), 범용 에디터 `MealKitEditor.exe`, 서드파티, 빌드 공통 설정(`cmake/`), 도구, 예제 게임 `samples/sandbox` | 엔진·에디터 개발, 결정론 테스트 |
| `D:\GameEngine\MealKit\<게임>` | `mealkit.project`, 게임 코드(`source/`), 에셋(`assets/`), 빌드(`build/`), **출시본(`export/<게임>/`)** | 게임 만들기 |

빌드 결과는 두 갈래다.

| 빌드 | 결과 | 쓰는 곳 |
|---|---|---|
| `build.bat Editor` | 엔진: `mealkit.dll` + `MealKitEditor.exe` / 프로젝트: `<게임>_game.dll`(게임 코드만, 수 초) | 에디터. 동적 CRT, 최적화 + 디버그 정보 |
| `build.bat` (Release) | `<게임>.exe` 하나 (엔진 정적 링크, 정적 CRT) | 출시, 결정론 테스트 |

- 에디터는 게임 DLL의 **복사본**을 불러오므로, 에디터를 켜 둔 채로 게임 코드를 다시 빌드할 수 있다. 새 DLL이 생기면 편집 모드에서 **자동으로 바꿔 끼운다(핫 리로드)**.
- 게임 DLL이 없거나 엔진(`mealkit.dll`)보다 오래되었으면 에디터가 프로젝트를 열 때 게임 코드를 먼저 빌드한다.
- 엔진과 게임 코드의 경계는 `mealkit::game::GameInstance`(`engine/src/mealkit/game/GameInstance.h`). 게임은 이것을 구현하고(엔진 서비스는 `GameContext&`로 받는다), `MEALKIT_GAME_MODULE(이름, 클래스)`로 에디터용 진입점을 내보낸다. 약속이 바뀌면 `kApiVersion`을 올린다(다른 번호의 DLL은 불러오지 않는다).
- 출시 exe는 게임 클래스를 직접 쓰므로 DLL 경계 비용이 없다.

엔진 저장소 자신도 예제 게임 프로젝트다(최상위 `mealkit.project`, 코드는 `samples/sandbox`, 에셋은 `assets/`). 새 프로젝트의 템플릿이자 엔진 결정론 테스트(`tools\mp_test.ps1`)의 대상이다.

### 프로젝트 열기, 새 프로젝트

- **프로젝트 관리자**: 엔진 폴더의 `editor.bat`(에디터가 없으면 먼저 빌드한다)이나 `build\Editor\bin\MealKitEditor.exe`를 그냥 실행하면 뜬다(고도 프로젝트 관리자와 비슷하다).
  [만들기]·[가져오기]·검색·언어(한국어/English). 목록에서 별(즐겨찾기), 아이콘·이름(= exe 이름, `tools\rename_project.ps1`)·폴더 경로(폴더 옮기기)를 눌러 고치고, 더블클릭으로 연다.
  고른 아이콘(`icon.png`, 프로젝트 설정 `icon`)은 출시 빌드 때 게임 exe 아이콘이 된다(없으면 밀키트 아이콘).
- 프로젝트 폴더의 `editor.bat` = `MealKitEditor.exe --project <그 폴더>`. 엔진 저장소의 예제 프로젝트는 `editor.bat sample [씬]`.
- 명령줄로 새 프로젝트:

```powershell
powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame          # → D:\GameEngine\MealKit\MyGame
powershell -ExecutionPolicy Bypass -File tools\new_project.ps1 -Name MyGame -Path E:\Games
```

예제 게임의 코드와 `assets/`를 복사하고 `templates/project`의 파일(`mealkit.project`, `CMakeLists.txt`, `build.bat`, `editor.bat`, `.vscode/`, `README.md`, `CLAUDE.md`)을 채운다.
엔진 위치는 프로젝트 `mealkit.project`의 `engine` 줄에 있고, 실제 빌드는 엔진의 `tools\build_project.bat`이 한다.
프로젝트 `CMakeLists.txt`는 `cmake/MealKit.cmake`를 include하고 `mealkit_add_game(...)` 한 번으로 게임 exe·게임 DLL·출시 타깃을 만든다.

### 출시 빌드 (export)

- 에디터: **빌드 → 출시 빌드** (`Ctrl+B`). 에디터를 켜 둔 채로 백그라운드에서 빌드하고 로그 창에 보여 준다. 저장 안 된 씬은 먼저 저장한다.
- 명령줄: 프로젝트 폴더에서 `build.bat Export`.
- 결과: `export\<게임>\` 에 `<게임>.exe`, `assets\`, `steam_api64.dll`, `THIRD_PARTY_NOTICES.txt` (앱 ID가 480이면 `steam_appid.txt`도). 이 폴더째로 배포한다.

## 작업 흐름 (VS Code + 에디터)

게임을 만들 때는 **게임 프로젝트 폴더**에서, 엔진을 고칠 때는 이 폴더에서 작업한다.

1. **에디터를 연다**: 프로젝트의 `editor.bat`, 또는 프로젝트 관리자. 처음에는 엔진 에디터와 게임 코드를 빌드하느라 몇 분 걸린다.
2. **VS Code로 프로젝트 폴더를 연다**(에디터는 켜 둔 채로). 추천 확장(C/C++)을 설치하라는 알림이 뜨면 설치한다.
3. 역할을 나눠 작업한다.

| 하고 싶은 일 | 어디서 |
|---|---|
| 맵 배치 (상자, 모델, 스폰 지점, 조명·안개) | 에디터 → `Ctrl+S` 로 `assets/scenes/*.scene` 저장 |
| 게임 규칙 (이동, 점프, 밀치기, 물리) | VS Code → 프로젝트의 `source/sim/` (엔진 예제는 `samples/sandbox/sim/`). 시스템별 파일, 실행 순서는 `World::step()` |
| 카메라, 조작, 화면에 그리는 것 | VS Code → 프로젝트의 `source/view/` (한 판 진행은 `source/app/GameSession.cpp`) |
| 엔진·에디터 기능 추가 | VS Code → 엔진 폴더의 `engine/src/mealkit/...` → `build.bat Editor` (에디터를 닫고) |
| 출시용 게임 만들기 | 에디터 → 빌드 → 출시 빌드 (`Ctrl+B`) → `export/<게임>/` |
| 3D 모델 만들기 | Blender → glTF(.glb)로 `assets/models/`에 내보내기 |

4. **게임 코드를 고친 뒤**: VS Code `Ctrl+Shift+B`(게임 코드 빌드) 또는 에디터 `Ctrl+R` → 에디터가 새 코드로 바꿔 끼운다. 에디터를 닫을 필요가 없다.
5. **디버깅**: VS Code `F5` → `Editor (debug game code)`(에디터 안의 게임 코드) 또는 `Game (Debug)`(출시 exe).
6. **멀티플레이 확인**: `Tasks: Run Task` → `Multiplayer Determinism Test` (PASS가 나와야 한다).

## 빠른 시작: 에디터

1. **씬 편집**: 뷰포트에서 물체를 클릭하거나 씬 트리에서 고른 뒤, 기즈모를 끌거나 인스펙터에 값을 넣는다.
2. **▶ 플레이 (F5)**: 지금 편집 중인 씬으로 바로 게임을 실행한다. `Esc`를 누르면 편집으로 돌아온다(씬은 바뀌지 않음). 게임 코드를 불러오기 전에는 비활성.
3. **Ctrl+S**: 저장. 게임과 멀티플레이는 프로젝트의 `assets/scenes/arena.scene`(`mealkit.project`의 `scene`, 에디터 [파일] > [프로젝트 설정])을 쓴다.
4. **Ctrl+R**: 게임 코드 빌드 (핫 리로드). **Ctrl+B**: 출시 빌드 (`export/<게임>/`). 빌드 메뉴에서 출시·프로젝트 폴더를 열 수 있다.

| 조작 | 기능 |
|---|---|
| 오른쪽 드래그 + `WASD` / `Q` `E` | 카메라 비행 (`Shift` 빠르게, 휠로 속도 조절) |
| 휠 / 가운데 드래그 | 앞뒤 이동 / 화면 이동 |
| 왼쪽 클릭 | 선택 (빈 곳을 클릭하면 해제) |
| `W` `E` `R` | 이동 / 회전 / 크기 기즈모 (`Ctrl` 누른 채 끌면 스냅) |
| `F` 또는 씬 트리 더블클릭 | 선택한 물체로 카메라 이동 |
| `Ctrl+D` / `Delete` | 복제 / 삭제 |
| `Ctrl+Z` / `Ctrl+Y` | 되돌리기 / 다시 실행 |
| `Ctrl+S` / `Ctrl+O` / `Ctrl+N` | 저장 / 열기 / 새 씬 |

물체 종류: **충돌 상자**(보이고 부딪힘), **장식**(보이기만 함), **스폰 지점**(플레이어 시작 위치, 게임에서는 안 보임).
충돌 상자와 장식에는 **모델**(glTF)을 붙일 수 있다. 씬 트리의 `+ 모델` 또는 인스펙터의 `모델 선택...`.

## 3D 모델 (glTF)

- `assets/models/` 아래에 `.glb`(권장) 또는 `.gltf`를 둔다. Blender: 파일 → 내보내기 → glTF 2.0, 형식 glTF Binary.
- 씬에는 `models/tree.glb`처럼 **assets 기준 상대 경로**로 저장된다.
- 불러올 때 meshoptimizer가 정점 캐시·오버드로·페치 순서를 최적화하고, 삼각형이 많으면 **LOD를 2단계 더** 만든다. 멀어지면 자동으로 가벼운 LOD를 쓴다.
- 색은 재질의 기본 색(baseColorFactor)과 정점 색을 쓴다. **텍스처는 아직 지원하지 않는다.**
- 충돌 상자에 붙인 모델은 **모델 정점의 볼록 껍질**로 충돌한다 (오목한 부분은 메워진다).
- 샘플 모델(나무, 바위, 고리)은 `python tools/make_sample_models.py`로 다시 만들 수 있다.

## 현재 상태 (v0.4)

| 모듈 | 내용 |
|---|---|
| Core | 로그, 고해상도 시계, 프레임 제한기, 수학(Vec3/Quat/Mat4, reversed-Z), PCG32 난수, FNV 해시, Tracy 마커 |
| Platform | Win32 창, Per-Monitor DPI, Raw Input, 경로 찾기, 자식 프로세스(에디터 빌드), Steam(SDK가 있을 때) |
| Render | DX12 포워드 렌더러: 메시별 인스턴싱(드로우 콜 = 메시 종류 수), 업로드 링 버퍼, 빌드 시점 셰이더 컴파일(DXC), 정점 색, 16/32비트 인덱스, PNG 스크린샷, Tracy GPU 시간 |
| Physics | Jolt Physics 래퍼: 상자/구/캡슐/볼록 껍질, 광선, 상태 해시. 결정론 빌드 |
| Asset | glTF 불러오기(cgltf) + 최적화·LOD(meshoptimizer) + 모델 캐시 |
| Net | 호스트-클라이언트(방장이 플레이어이자 중계자, 전용 서버 없음) 결정론 입력 락스텝: 틱당 입력 8바이트만 전송, 상태 해시로 디싱크 감지, 최대 8명. 전송: UDP(중복 전송, TCP 안 씀) 또는 Steam 릴레이. 빌드·씬·모델·튜닝이 다르면 접속 거부. 맞는 장르와 한계는 `docs/ROADMAP.md` "네트워크 방식" |
| Scene | 물체(상자/모델, 충돌/장식/스폰)와 환경. 사람이 읽을 수 있는 `.scene` 텍스트 |
| Editor | 범용 에디터 `MealKitEditor` (Dear ImGui + ImGuizmo): 프로젝트 관리자, 게임 DLL 핫 리로드, 도킹 패널, 씬 트리, 인스펙터, 기즈모, 클릭 선택, 되돌리기, 모델 선택, 플레이 모드, 게임 코드·출시 빌드 |

측정값 (Intel Core Ultra 5 125U 내장 그래픽, 1280×720, 수직동기 끔, Release, 상자 2,000개 + 모델 9개):

| 항목 | 값 |
|---|---|
| 프레임 | 약 3,500 fps (0.28 ms), 드로우 콜 4회 |
| 시뮬레이션 (Jolt, 틱당) | 떨어지는 중 약 2.5 ms → 쌓여서 잠든 뒤 약 0.9 ms |
| 4인 네트워크 (상자 8,000개) | 호스트 송신 합계 약 5 KB/s, 물체 수와 무관 |
| 실행 파일 | 출시 게임 약 1.4 MB (정적 CRT, 재배포 패키지 불필요). 에디터: `MealKitEditor.exe` 1.8 MB + `mealkit.dll` 2.7 MB, 게임 DLL 약 0.1 MB |

## 요구사항

- 실행: Windows 10 1803 이상, **Intel 11세대(Xe) 내장 그래픽 이상** (Direct3D 12, Shader Model 6.6 + 리소스 바인딩 티어 3 — 시작 때 검사하고, 없으면 드라이버 버전과 함께 이유를 알려 주고 끝낸다. 텍스처 작업부터 6.6 bindless를 쓴다), SSE4.2 CPU
  - 최소 사양 결정과 이유: [docs/ROADMAP.md](ROADMAP.md) "결정 기록"
- 빌드: Visual Studio 2022 또는 Build Tools 2022 (C++ 데스크톱 워크로드, CMake 포함), Python 3 (샘플 모델 생성 시)

## 빌드

```bat
build.bat Editor     :: 범용 에디터: build\Editor\bin\MealKitEditor.exe + mealkit.dll + sandbox_game.dll
build.bat            :: Release: build\Release\bin\sandbox.exe (출시용 게임, 엔진 정적 링크)
build.bat Debug      :: Debug (D3D12 디버그 레이어 자동 활성화)
build.bat Profile    :: Release + Tracy 프로파일링
build.bat Export     :: 출시 폴더 export\sandbox\
build.bat ClangCL    :: clang-cl Release: build\ClangCL\bin (MSVC 빌드와 결정론 비교용, 출시하지 않는다)
```

`ClangCL`은 Visual Studio의 "C++ Clang tools for Windows" 구성 요소가 필요하다(PATH의 clang-cl, 없으면 미리 보기 버전을 포함한 Visual Studio 설치에서 찾는다).

게임 프로젝트에도 같은 인자를 받는 자기 `build.bat`이 있다 (빌드 폴더는 프로젝트 안의 `build\`). 프로젝트의 `build.bat Editor`는 게임 DLL만 만들고, 엔진 에디터 빌드가 오래되었으면 그것부터 빌드한다.
엔진(`mealkit.dll`)을 다시 빌드할 때는 에디터를 닫는다(실행 중이면 파일이 잠겨 링크가 실패한다). 게임 DLL은 에디터가 켜져 있어도 된다.

## 게임 실행

```bat
sandbox.exe                          :: 혼자 플레이
sandbox.exe --host                   :: 방 만들기 (UDP 27015). 로비에서 Enter로 시작
sandbox.exe --join 192.168.0.10      :: 참가
sandbox.exe --steam                  :: Steam 친구 로비 만들기 (Steamworks SDK 필요)
sandbox.exe --scene my.scene         :: 다른 씬으로 플레이
sandbox.exe --system-info            :: CPU·GPU 기능 출력 (최소 사양 미달이면 종료 코드 2)
sandbox.exe --help                   :: 전체 옵션
```

- 모든 플레이어가 **같은 실행 파일, 같은 씬, 같은 모델 파일**을 써야 한다. 다르면 접속이 거부된다(결정론 보장).
- UDP로 다른 PC에서 접속하려면 호스트 방화벽에서 `sandbox.exe`를 허용해야 하고, 인터넷 너머라면 포트 포워딩이 필요하다. Steam을 쓰면 둘 다 필요 없다.
- 같은 PC에서 여러 창으로 테스트할 때는 `--host --local`을 쓰면 방화벽 확인 창이 뜨지 않는다.

조작: `WASD` 이동, `Space` 점프, `왼쪽 클릭` 밀치기, `오른쪽 드래그` 카메라, `휠` 줌, `V` 수직동기, `F12` 스크린샷, `Esc` 종료

## Steamworks 설정

Steamworks SDK는 Valve 약관상 저장소에 넣지 않았다. 직접 받아서 넣으면 CMake가 자동으로 감지한다.

1. https://partner.steamgames.com/ 에 Steam 계정으로 로그인 → **SDK 다운로드** (`steamworks_sdk_xxx.zip`).
2. 압축을 풀어 `sdk` 폴더째로 `third_party/steamworks/sdk`에 둔다. 아래 두 파일이 있어야 한다.
   - `third_party/steamworks/sdk/public/steam/steam_api.h`
   - `third_party/steamworks/sdk/redistributable_bin/win64/steam_api64.lib`
3. `build.bat`을 다시 실행한다. 빌드 로그에 `Steamworks SDK found`가 보이면 켜진 것이다. `steam_api64.dll`과 `steam_appid.txt`는 자동으로 실행 파일 옆에 놓인다. 앱 ID는 `mealkit_add_game(... STEAM_APP_ID <id>)` (기본 480).
4. Steam 클라이언트를 켠 상태에서 `sandbox.exe --steam` → 로비가 만들어지고 친구 초대 창이 뜬다(`I`로 다시 열기). 친구가 초대를 수락하면 게임이 `+connect_lobby`로 실행되어 자동으로 들어온다.

- 개발 중에는 Valve의 테스트 앱 ID **480(Spacewar)** 을 쓴다. 출시할 때는 프로젝트 `CMakeLists.txt`의 `STEAM_APP_ID`를 Steam에서 받은 내 앱 ID로 바꾼다. 480이 아니면 출시 폴더에 `steam_appid.txt`를 넣지 않는다(Steam이 실행해 준다).
- Steam 백엔드(`engine/src/mealkit/online/OnlineSteam.cpp`, 게임은 `online::Service`만 쓴다)는 SDK v1.65로 **컴파일·링크까지 확인했다.** Steam 클라이언트를 켜고 실제 로비·릴레이로 접속하는 것은 아직 검증하지 못했다.
- `third_party/steamworks/`는 `.gitignore`에 들어 있다.

## 프로파일링 (Tracy)

1. `build.bat Profile` → `build\Profile\bin\sandbox.exe` 실행.
2. Tracy 프로파일러 앱(v0.14.1, https://github.com/wolfpld/tracy/releases)을 켜고 `127.0.0.1`에 연결한다.
3. 프레임별 CPU 구간(`Simulation`, `Physics step`, `Network`, `Render`, `Editor UI`)과 **GPU 프레임 시간**이 보인다.

- 일반 빌드에서는 `MEALKIT_ZONE("이름")` 마커가 아무 코드도 만들지 않는다. 측정하고 싶은 함수에 마음껏 넣어도 된다.
- 프로파일 빌드는 프로파일러가 붙었을 때만 수집하고(`TRACY_ON_DEMAND`), 127.0.0.1에서만 접속을 받는다.

## 테스트

```powershell
powershell -ExecutionPolicy Bypass -File tools\mp_test.ps1 -Clients 3 -Ticks 1800
```

호스트 1개와 클라이언트 N개를 봇 입력으로 실행하고, 모든 피어가 같은 틱에서 같은 상태 해시를 내는지 확인한다.
클라이언트 i는 물리 스레드를 i개 쓰므로, **스레드 수가 달라도 결과가 같은지**도 함께 확인된다.

```powershell
powershell -ExecutionPolicy Bypass -File tools\mp_test.ps1 -Clients 3 -Ticks 900 -ClientExe build\ClangCL\bin\sandbox.exe
```

MSVC 빌드 호스트에 clang-cl 빌드 클라이언트를 붙여 **컴파일러가 달라도 결과가 같은지** 본다(`-Exe`와 바꿔 반대 방향도).
보통은 실행 파일이 다르면 접속이 거부되므로, 이때 호스트는 시험용 `--any-build`로 다른 빌드를 받는다.

## 외부 라이브러리

| 라이브러리 | 버전 | 라이선스 | 쓰는 곳 | 이유 |
|---|---|---|---|---|
| Jolt Physics | v5.6.0 | MIT | 엔진 | 결정론 빌드 지원, 멀티스레드, 빠름 |
| cgltf | v1.15 | MIT | 엔진 | 파일 하나짜리 glTF 로더 |
| meshoptimizer | v1.3 | MIT | 엔진 | 모델 최적화와 LOD 생성 |
| Tracy | v0.14.1 | BSD-3 | 프로파일 빌드만 | CPU/GPU 프로파일링 |
| Dear ImGui | 1.92.9b (docking) | MIT | 에디터만 | 에디터 UI |
| ImGuizmo | 18cef5e | MIT | 에디터만 | 3D 기즈모 |
| Steamworks SDK | (직접 받음) | Valve 약관 | 엔진 (있을 때) | 로비, 친구 초대, 릴레이 |

- Jolt는 `CROSS_PLATFORM_DETERMINISTIC`으로, 사무용 CPU(펜티엄·셀러론 포함)에서도 돌도록 **SSE4.2까지만** 쓰게 빌드한다(AVX/FMA 끔).
- 게임을 배포할 때는 `THIRD_PARTY_NOTICES.txt`를 함께 넣는다. 라이브러리를 바꾸면 `third_party/VERSIONS.txt`와 이 표를 고친다.

## 구조

```
engine/
  shaders/Forward.hlsl       인스턴스 변환 + 정점 색 + 하프 램버트 + 반구 환경광 + 안개 + 선택 강조
  src/mealkit/core/            Types, Log, Clock, Math, Random, Hash, Profile
  src/mealkit/platform/        Window, Input, Paths, Process, System, DynamicLibrary, FileDialog, Image (헤더 + 공통 부분)
  src/mealkit/platform/win32/  OS를 부르는 Windows 구현 (Win32, Winsock, WIC, 대화상자)
  src/mealkit/platform/posix/  헤드리스용 Linux·macOS 최소 구현 (시간, 파일, UDP, 로그)
  src/mealkit/online/          Online (계정·로비·초대·릴레이): Steam 백엔드, SDK 없을 때의 None 백엔드
  src/mealkit/render/          Renderer, Mesh, DrawList
  src/mealkit/physics/         PhysicsWorld (Jolt)
  src/mealkit/asset/           ModelCache (cgltf + meshoptimizer)
  src/mealkit/net/             Socket, Transport, Lockstep
  src/mealkit/scene/           Scene (.scene 읽기/쓰기)
  src/mealkit/game/            GameInstance, GameContext (엔진·에디터와 게임 코드의 경계, MEALKIT_GAME_MODULE)
  src/mealkit/sim/             Pool<T>, TickTimer, TickEvents (게임 sim/이 쓰는 결정론 도구)
  src/mealkit/editor/          MealKitEditor: EditorMain, ProjectManager, Project, GameModuleHost(DLL 핫 리로드), Editor, ImGuiLayer
  tools/SimCheck.cpp         게임 sim/ 폴더의 결정론 규칙 검사 (빌드 때 mealkit_add_game이 실행)
samples/sandbox/             예제 게임 = 새 프로젝트 템플릿 (프로젝트에서는 source/ 아래)
  sim/                       결정론 시뮬레이션: World(상태, step 순서, hash), Systems.h, Pawns(슬롯·컨트롤러·Pawn·AI), Props, Objects, Tuning
  view/                      Camera, LocalInput(조작 → GameInput), Draw, Effects(틱 이벤트 → 화면 효과) (sim/ 상태는 읽기만)
  app/                       GameSession(한 판 진행, GameInstance 구현), main.cpp(출시 exe), GameModule.cpp(에디터 DLL 진입점)
mealkit.project                엔진 저장소를 예제 게임 프로젝트로 열 때 쓰는 프로젝트 파일
assets/scenes, assets/models 씬과 모델
third_party/                 외부 라이브러리 (VERSIONS.txt)
cmake/MealKit.cmake            빌드 공통 설정 + mealkit_add_game() (엔진 저장소와 게임 프로젝트가 같이 쓴다)
cmake/MealKitExport.cmake      출시 폴더 만들기 (game_export 타깃)
cmake/FixExportsDef.cmake    mealkit.dll 내보내기 목록에서 MSVC CRT 약한 별칭 줄 제거 (Editor 빌드)
templates/project/           새 게임 프로젝트의 빌드 파일 템플릿
tools/                       build_project.bat (공통 빌드), new_project.ps1 (새 프로젝트), upgrade_project.ps1 (프로젝트를 새 엔진 템플릿으로),
                             check.ps1 (전체 검증), mp_test.ps1 (멀티플레이 테스트), make_sample_models.py
VERSION                      엔진 버전 (mealkit.project의 engineVersion과 비교)
.vscode/                     빌드 작업, 디버그 실행 구성
```

### 프레임 흐름

1. `Window::pumpMessages`: 입력 수집
2. `LockstepSession::update`: 패킷 수신, 입력 확정, 송신
3. 시뮬레이션: 30Hz 고정 틱. **확정된 틱만** 실행하고(Jolt는 틱당 2단계), 실행 직후 로컬 입력을 `틱 + inputDelay`용으로 제출한다.
4. 렌더링: 직전 틱과 현재 틱을 보간해 `DrawList`에 모으고 메시별로 한 번씩 그린다.

## 결정론 규칙 (시뮬레이션 코드)

락스텝은 모든 PC가 비트 단위로 같은 계산을 해야 동작한다. 게임의 시뮬레이션 코드는 `sim/` 폴더에 두고, `World::step()`이 시스템 함수를 정해진 순서로 직접 부른다.
`sim/` 파일은 빌드 전에 **SimCheck**(`engine/tools/SimCheck.cpp`)가 검사해, 아래 규칙과 `docs/CODING_MODEL.md`의 금지 목록
(CRT 수학 함수, 실제 시간, `rand`, `new`/`delete`, `virtual`, 코루틴·스레드, `unordered_map`, 가변 전역·`static` 변수, 렌더러·플랫폼·`view/`·`app/` 헤더)을 어기면
`파일(줄,열): error SIMnnn`과 대신 쓸 것을 출력하고 빌드를 멈춘다.

- 사칙연산과 `sqrt`만 쓴다. `sin/cos/atan2/pow/exp` 같은 CRT 함수는 CPU에 따라 다른 경로(FMA)를 탈 수 있다. 각도 계산은 `mealkit/core/DetMath.h`의 `detSin`/`detCos`/`detAtan2`/`detQuatFromAxisAngle`을 쓰거나, 입력 단계에서 계산해 양자화해서 보낸다.
- 컴파일 옵션은 `/fp:precise`로 고정한다. `/fp:fast`, `/fp:contract`, `/arch:AVX2`는 쓰지 않는다. Clang·GCC(다른 플랫폼)는 `-ffp-contract=off`이고 `-ffast-math`를 쓰지 않는다.
- 난수는 `Pcg32`(시드 기반)만 쓴다.
- 물리 바디는 모든 피어에서 같은 순서로 만들고 지우고, 힘도 같은 순서로 준다.
- 순회 순서를 고정한다. 해시맵을 순회하지 않고, 포인터 주소에 의존하지 않는다.
- 실제 시간, 카메라, 프레임레이트, 렌더링 LOD를 시뮬레이션이 읽지 않는다.
- 틱을 넘어 남는 상태는 `WorldState` 안에 `MEALKIT_FIELD`로 선언한다. `World::hash()`와 스냅샷이 선언을 그대로 쓰므로 빠뜨릴 수 없다.

## 보이스 챗

엔진 코어에는 넣지 않았다. 친구끼리는 **디스코드 통화를 권장**한다.
성능 비용은 작지만(인코딩은 CPU 코어 하나의 1~2%, 말하는 사람당 약 3 KB/s), 마이크·에코 제거 등 개발 비용이 크다.
필요해지면 Steamworks 위에 **Steam Voice를 선택 모듈로** 붙인다.

## 다음 단계

자세한 계획과 순서는 [docs/ROADMAP.md](ROADMAP.md), 끝낸 것의 상세는 [docs/CHANGELOG.md](CHANGELOG.md). 다음은 1단계(v0.5, 게임이 되게 하는 최소 기능)다.

1. 위험 확인: 최소 사양(11세대) 실기 측정, 다른 PC 사이 결정론, Steam 실검증
2. 플랫폼 계층(`mealkit/platform/`): 오디오·입력이 OS API를 고르기 전에
   이어서 온라인 서비스 계층(Steam 백엔드부터, 크로스플레이는 3단계 EOS), 헤드리스 시뮬레이션, CI 빌드 매트릭스(GitHub Actions: Windows MSVC·clang-cl, Linux x64·macOS ARM64 헤드리스 빌드의 틱별 해시 비교), 저장소 공개 범위 결정(라이선스는 MIT로 정함)
3. 데이터와 코드 분리: ~~프로젝트 설정~~ → 화면용 튜닝(+ 시간 값은 초) → 프로젝트 폴더 구조 강제 → 예제 데이터 정리 → 정의 데이터 표
4. 그다음 순서 (각 항목은 기준 샘플에서 실제로 써야 끝난다):
   - 오디오 → 이벤트 → 연출 표
   - 입력 액션 매핑 → 게임패드 → 확정 명령 채널 → 디버그 UI(ImGui) → 로컬라이제이션
   - 게임 UI(`.ui` 런타임 + 미리보기. Penpot 플러그인은 디자이너가 생길 때)
   - 설정 화면, 세이브, 매치 시작 데이터(로드아웃), 기본 화면 흐름
   - 네트워크 흉내, 입력 지연 자동 조절, 내 캐릭터 겉모습 예측, 틱레이트 설정, 버전 표시·접속 거부 이유

## 라이선스

MealKit 엔진은 [MIT 라이선스](../LICENSE)다. 엔진이 쓰는 외부 라이브러리의 라이선스는 [THIRD_PARTY_NOTICES.txt](../THIRD_PARTY_NOTICES.txt)와 위 표에 있다.
이 엔진으로 만든 게임의 출시본에는 MealKit의 저작권 고지와 라이선스 문구를 함께 넣는다.
