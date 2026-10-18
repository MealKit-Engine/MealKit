# MealKit 변경 기록

끝낸 로드맵 항목의 구현 상세(무엇을 만들었고, 왜, 어떻게, 남은 것). 앞으로의 계획과 순서는 `docs/ROADMAP.md`, 설계 답은 `docs/CODING_MODEL.md`. 결정의 이유와 넣지 않거나 미룬 기능은 `docs/DECISIONS.md`.
로드맵 항목을 끝내면 상세를 여기로 옮기고 ROADMAP에는 한 줄만 남긴다. 최신 버전이 위에 온다.

## v0.4.3 — 1단계 진행 중 (2026-10-16)

### 고침: 에디터가 멀쩡한 게임 DLL을 "엔진보다 오래됨"으로 보던 문제 (2026-10-18)
- **현상**: `GameModuleHost::olderThanEngine`이 게임 DLL과 `mealkit.dll`의 수정 시각을 비교했다. 엔진 `.cpp`만 바뀌면 `mealkit.dll`은 다시 링크되지만 내보내기가 같아 import lib이 그대로라 Ninja가 게임 DLL을 다시 링크하지 않는다.
  그래서 에디터가 시작할 때마다 게임 코드 빌드를 했고, 그 빌드가 실패하면 이전 DLL로 돌아가지 못했다(check.ps1 "Editor keeps the old DLL on failure" 실패).
- **판정**: 엔진 시각 = 엔진 import lib(`build\Editor\engine\mealkit.lib`, 링커가 내보내기가 바뀔 때만 다시 쓴다)과 엔진 헤더(`engine/src/mealkit/**/*.h`, 게임 코드가 쓰지 않는 `editor/` 제외) 중 가장 새것.
  헤더를 넣은 것은 구조체 배치·인라인 함수처럼 내보내기 없이 바뀌는 경우 때문이다. import lib이 없으면 예전처럼 `mealkit.dll` 시각을 쓴다. 게임 프로젝트도 같은 엔진 빌드의 lib(`MealKitTargets.cmake`의 `IMPORTED_IMPLIB`)과 `MEALKIT_ENGINE_DIR` 헤더를 본다.
  게임 모듈 경계(`GameModule`, `kApiVersion`)는 바꾸지 않았다. 헤더를 내용 변화 없이 저장만 해도 다시 빌드를 요구한다(보수적).
- **검증**: 엔진 `.cpp`에 주석만 더해 `mealkit.dll`만 다시 링크(lib은 그대로)한 상태에서 `check.ps1` 17개 통과, 에디터가 빌드 없이 게임 DLL을 불러옴.
  `core/Types.h` 시각을 새로 하면 에디터가 시작 때 게임 코드 빌드를 시작했고, 엔진을 다시 빌드하자 게임 DLL도 다시 링크되어 바로 불러왔다.

### [x] 플랫폼 계층 (`mealkit/platform/`) (완료 2026-10-18)
SDL3 평가(창·입력·게임패드·오디오 장치)는 하지 않고 1단계 "오디오" 항목의 첫 일로 옮겼다(`docs/DECISIONS.md` 2026-10-18). 창·입력은 Win32 그대로다.
- **규칙**: 헤더는 기능 자리에 그대로 두고(게임 코드의 include가 바뀌지 않는다), OS API를 부르는 구현 `.cpp`만 `platform/win32/`·`platform/posix/`로 옮겼다. CMake가 `WIN32`로 하나를 고른다.
  **옮긴 것**: `core/Clock.cpp` → `win32/Clock.cpp`, `net/Socket.cpp`의 소켓 부분 → `win32/Socket.cpp`(주소 표시는 `net/Socket.cpp`에 남김),
  `platform/Paths.cpp`는 std::filesystem 부분(`platform/Paths.cpp`)과 OS 부분(`win32/Paths.cpp`)으로 나눔, `Process.cpp`·`Window.cpp` → `win32/`, `editor/Image.*` → `platform/Image.h` + `win32/Image.cpp`(WIC, `namespace mealkit`).
- **새 인터페이스**: `Paths.h`에 `openFile`(UTF-8 경로 fopen), `fileCreationTime`, `formatFileTime`, `userDataDirectory`(%LOCALAPPDATA% / XDG / Application Support), `documentsDirectory`.
  `System.h`에 `debugOutput`, `debuggerAttached`, `debugBreak`, `showErrorDialog`, `exitProcess`, `processId`. 새 헤더 `DynamicLibrary.h`(게임 DLL), `FileDialog.h`(`openFileDialog`·`saveFileDialog`·`pickFolderDialog`).
  에디터 전용(DLL·대화상자·그림)은 Windows 구현만 있다.
- **바뀐 호출처**: `Log.cpp`(SRWLOCK → `std::mutex`, 대화상자·종료는 `System.h`), `Scene.cpp`·`ModelCache.cpp`(`openFile`), `GameModuleHost.cpp`(`DynamicLibrary`, `processId`, `fileWriteTime`),
  `Project.cpp`(`userDataDirectory`), `ProjectManager.cpp`·`Editor.cpp`(파일 대화상자·파일 시각·문서 폴더를 platform으로, 각자 갖던 UTF-8↔UTF-16 변환 삭제). 프로젝트 아이콘 고르기는 공용 대화상자(`GetOpenFileName`)로 바뀌었다.
- **POSIX 최소 구현** (`platform/posix/`): `Clock`(clock_gettime + nanosleep), `Socket`(BSD 소켓, 논블로킹), `Paths`(/proc/self/exe·_NSGetExecutablePath, fsync + rename 원자적 쓰기, 나노초 파일 시각),
  `System`(대화상자 없음, `_Exit`). **이 PC에 Linux 도구가 없어 한 번도 컴파일하지 않았다** — 헤드리스·CI 항목에서 처음 빌드하며 고친다. 창·입력·DLL·대화상자·그림은 5단계.
- **검사** `tools\check_platform.ps1`(check.ps1 "Platform layer" 단계): `engine/src/mealkit`(`platform/win32|posix` 밖)과 `samples/`에서 OS 헤더 include와 대표 Win32·POSIX 함수 호출을 찾는다(대소문자 구분).
  예외는 RHI 전의 `render/Renderer.cpp`, `editor/ImGuiLayer.cpp` 두 개. 일부러 `<windows.h>`를 넣은 파일로 실패하는 것을 확인했다.
- **검증**: Release·Editor·ClangCL 빌드, `check.ps1` 17개 통과. 에디터의 파일·폴더 대화상자는 직접 눌러 보지 않았다.
  검증 중 "Editor keeps the old DLL on failure"가 두 번 실패했다: 이번 작업 중 `mealkit.dll`이 내보내기 변화 없이 다시 링크돼 게임 DLL보다 새것이 됐고, `olderThanEngine`(파일 시각 비교)이 멀쩡한 게임 DLL을 "엔진보다 오래됨"으로 봤다.
  게임 DLL을 다시 링크한 뒤 통과. 판정 방식은 바로 위 "고침" 항목에서 고쳤다.

### [x] Steam 실검증 (완료 처리 2026-10-18, 미검증)
**실제 Steam 접속 시험은 하지 않았고, 사용자 결정으로 문제없이 통과한 것으로 가정하고 항목을 닫았다.** Steam 백엔드는 여전히 컴파일·링크만 확인된 상태다. 측정값은 없다.
- **가정하고 넘긴 것 (실제로 하면 결과를 여기에 적는다)**: 두 계정으로 `--steam` → 초대 → `+connect_lobby` 참가 → 900틱 이상 디싱크 없음, 끊김·재시도·NAT 로그, 릴레이 RTT(입력 지연 100 ms 가정과 맞는지),
  Steam 오버레이가 DX12 스왑체인 위에 뜨는지, Steam Deck·Proton에서 그림·입력·결정론.
- **가정이 틀리면**: 릴레이 RTT가 크면 입력 지연 자동 조절(1단계)을 당긴다. 접속 자체가 안 되면 `online/OnlineSteam.cpp`의 세션 수락·전송 옵션부터 본다.

### [x] 온라인 서비스 계층 (`mealkit/online/`, Steam·없음 백엔드) (완료 2026-10-18)
- **왜**: 로비·초대·릴레이가 `mealkit/platform/Steam.h`(`steam::`)에 직접 묶여 있었다. 크로스플레이(EOS)와 Steam이 없는 플랫폼을 더할 자리를 먼저 만든다.
- **`online::Service`** (`mealkit/online/Online.h`): 계정(`localUserId`, `displayName`), 친구 전용 로비(`createLobby`/`joinLobby`/`leaveLobby`/`lobbyState`/`isLobbyOwner`/`lobbyMemberCount`),
  초대(`inviteFriends`, `takeJoinRequest`), 방장 주소(`lobbyOwnerAddress()` → `net::Address`), 플랫폼 릴레이 전송(`createTransport()` → `net::Transport`).
  전역 함수였던 `steam::init()` 등을 객체 하나로 모았다(main이 만들어 인자로 넘긴다, CODING_MODEL "엔진 기능에 어떻게 접근하나"). SDK가 프로세스 전역이라 프로세스당 하나.
- **백엔드는 빌드 때 하나** (CODING_MODEL "그래픽 API와 OS 기능"과 같은 방식, 가상 함수 계층 없음): `OnlineSteam.cpp`(SDK가 있는 게임 빌드, `MEALKIT_STEAM=1`), `OnlineNone.cpp`(SDK가 없거나 Editor 빌드).
  `Service::backend()`/`backendName()`으로 이 빌드의 백엔드를 안다. "없음"은 로비가 없고 접속은 지금처럼 `UdpTransport`(IP 직접)다.
- **sandbox**: `--steam`·`--steam-join`·`+connect_lobby` 경로가 `online::Service`만 쓴다(옵션 이름은 그대로). Steam 코드는 옮기기만 했고 동작은 같다.
- **지운 것**: `mealkit/platform/Steam.h`, `Steam.cpp`, `SteamStub.cpp`. 이것을 직접 쓰던 게임 코드는 `online::Service`로 바꿔야 한다(템플릿은 그대로라 엔진 버전은 0.4.3 그대로. 예제 코드를 쓰는 프로젝트는 `upgrade_project -Source`).
- **이번에 넣지 않은 것**: 업적·통계·리더보드(4단계 "업적·리더보드·통계"), 클라우드 세이브(1단계 "세이브"), 백엔드 여럿 같이 켜기(Steam 친구 초대 + EOS 로비, 3단계 EOS). 계획한 기능 표:

  | 기능 | Steam | EOS (3단계) | 콘솔 플랫폼 서비스 (5단계) |
  |---|---|---|---|
  | 계정 | Steam ID | Product User ID (EOS Connect로 Steam·콘솔·기기 ID 로그인, Epic 계정 불필요) | 플랫폼 계정 |
  | 친구·초대 | Steam 오버레이 | 플랫폼 친구 + EOS 로비 초대 | 플랫폼 |
  | 로비·P2P 릴레이 | Steam 로비·릴레이 (Steam끼리) | EOS 로비·P2P (**크로스플레이 매치는 항상 이것**) | EOS |
  | 업적·통계·리더보드 | Steam | EOS | 플랫폼 (트로피 등) |
  | 클라우드 세이브 | Auto-Cloud (설정만) | Player Data Storage | 플랫폼 세이브 |

- **검증**: Release(Steam SDK 있음)·ClangCL·Editor(없음 백엔드) 빌드, `check.ps1` 통과(`mp_test` 포함). **실제 Steam 접속은 여전히 미검증**(위 "Steam 실검증"은 가정으로 닫았다).

### [x] 최소 사양 실기 측정과 다른 PC 결정론 (완료 처리 2026-10-18, 실기 미검증)
코드로 할 수 있는 부분만 실제로 했다. **아래 실기 시험은 하지 않았고, 사용자 결정으로 문제없이 통과한 것으로 가정하고 항목을 닫았다.** 측정값은 없다.
- **가정하고 넘긴 것 (실제로 하면 결과를 여기에 적는다)**:
  - 11세대(Xe-LP) 실기의 `sandbox.exe --system-info` 출력, Vulkan 버전·descriptor indexing(`vulkaninfo --summary`), `--bot` 프레임·틱 시간 → 성능 예산 표 "현재" 값은 여전히 Core Ultra 기준이다.
  - Intel PC ↔ AMD PC UDP 900틱 이상 디싱크 없음(시작 로그 `CPU:` 줄, MSVC·clang-cl 섞기 `--any-build`). Windows on ARM.
- **가정이 틀리면**: SM 6.6이 없으면 2단계 텍스처 방식(bindless)을 다시 정한다. CPU 사이 디싱크면 디싱크 진단 도구(3단계)를 당겨 원인을 찾고 고정소수점 보류를 다시 본다.
  CI 빌드 매트릭스(1단계)가 생기면 OS·CPU 사이 해시 비교는 매 커밋 자동으로 확인된다.
- **GPU 검사** (`render::queryGpu`, `render::gpuMinimumError`, `Renderer::gpu()`/`initError()`): 어댑터 이름, 사용자 모드 드라이버 버전(`CheckInterfaceSupport`, 장치 관리자와 같은 네 자리),
  vendor·device ID, 메모리, 기능 수준, 최고 셰이더 모델(런타임이 아는 것까지 6.9부터 내려가며 묻는다), 리소스 바인딩 티어, wave 폭, 메시 셰이더 티어(참고용), UMA.
  시작 로그 한 줄: `GPU: <이름> | driver <버전> | feature level 12_2 | SM 6.8 | binding tier 3 | unified (UMA) memory`.
  **SM 6.6과 바인딩 티어 3이 없으면 시작하지 않는다**(전에는 6.0 검사). 오류 창에 GPU 이름·드라이버 버전·"드라이버를 업데이트하라"·최소 사양(Intel 11세대)을 보인다.
  지금 셰이더는 6.0만 쓰지만 최소 사양 결정(2026-10-15)대로 미리 막아, 2단계 bindless 때 지원하지 않는 PC가 새로 생기지 않게 한다.
- **`sandbox.exe --system-info`**: 창 없이 CPU 이름과 위 GPU 정보를 출력하고 끝낸다. 최소 사양 미달이면 종료 코드 2. 실기 측정 결과를 그대로 옮겨 적는 용도.
  CPU 이름은 `mealkit/platform/System.h`의 `cpuName()`(CPUID 브랜드 문자열, x64만). sandbox 시작 로그에도 `CPU:` 줄을 남긴다(다른 PC 결정론 기록용).
- **`build.bat ClangCL`**: clang-cl로 Release(정적 CRT)를 `build\ClangCL`에 만든다. clang-cl은 PATH, 없으면 vswhere(`-prerelease` 포함)로 "C++ Clang tools" 구성 요소를 찾는다.
  clang에는 `/fp:precise` 대신 `-ffp-contract=off`를 준다(clang의 precise는 식 안 곱셈-덧셈 합치기를 허용한다). Jolt의 Windows 분기는 clang-cl에 그 옵션을 주지 않아 `cmake/MealKit.cmake`가 Jolt 타깃에도 건다.
  링크 시간 최적화는 끈다(lld-link가 필요해진다). `/W4` 경고 0개. exe 1.79 MB(MSVC 1.62 MB). 출시하지 않는 비교용 빌드다.
- **`mp_test.ps1 -ClientExe <exe>`**: 클라이언트만 다른 실행 파일로 돌린다. 호스트는 `--any-build`(시험 전용: 빌드 ID가 달라도 받는다, `LockstepSession::setAcceptOtherBuilds`)로 연다.
  빌드 ID는 지금 `Lockstep.cpp`의 컴파일 시각이라 컴파일러가 다르면 항상 다르다. 정식 비교 방식(시뮬레이션 버전 + 결정론 그룹)은 "버전 표시와 접속 거부 이유"에서 정한다.
- **결과 (Intel Core Ultra 5 125U, 같은 PC, clang 22.1.3 / MSVC 2022)**:
  `detmath-bits` 같음(`f67a477cb2496808`). MSVC 호스트 + clang-cl 클라이언트 3개, 반대 방향 모두 900틱 PASS이고 해시도 MSVC끼리와 같다(`5bcaa500ce729a6d`).
  같은 시드 혼자 900틱 해시 같음, clang-cl 빌드 단독 `SHADOW ok`·`REWIND ok`. `check.ps1` 통과(첫 실행에서 "Editor play (3 players)"가 한 번 3번째 플레이어 접속 전에 끝났고, 그 단계만 세 번·전체 다시 돌려 통과).

### README 줄이기 (2026-10-16)
- **README**: 영어로 한 줄 소개 + 특징 5개 + 문서 링크 + 라이선스만 남겼다. 사용 설명은 나중에 별도 문서 프로젝트(고도의 godot-docs처럼)로 다시 쓴다.
- **`docs/GUIDE.md`**: 예전 README 전체를 그대로 옮긴 임시 문서(한국어). README를 가리키던 곳(CLAUDE.md, ROADMAP, SimCheck 주석·메시지, SteamStub·sandbox 로그, `.gitignore`, `CMakeLists.txt`, 템플릿 README)을 이 문서로 바꿨다.
- **템플릿**: `templates/project/README.md`의 결정론 규칙 링크가 바뀌어 엔진 버전 0.4.2 → 0.4.3.

## v0.4.2 — 1단계 진행 중 (2026-10-16)

### [x] 고도식 프로젝트 관리자 + 에디터 한국어/영어 (완료 2026-10-16)
- **프로젝트 관리자** (`editor/ProjectManager.cpp`, 고도 프로젝트 관리자를 따름): 위 막대 [만들기](새 프로젝트 창) · [가져오기](폴더를 목록에 더함) · 검색(이름·경로) · 언어 고르기.
  목록 줄마다 즐겨찾기 별(맨 위로 정렬, 그다음 최근에 고친 순), 게임 아이콘(눌러서 그림 고르기), 게임 이름(눌러서 고치기), 폴더 경로(눌러서 고치기), 오른쪽에 흐리게 개발 시작일·마지막 수정일.
  더블클릭으로 열기, 오른쪽 클릭 메뉴(열기, 탐색기에서 보기, 아이콘 바꾸기, 목록에서 제거). 창 제목 "MealKit Engine - 프로젝트 매니저". `editor.bat`을 인자 없이 실행하면 뜬다(`editor.bat sample`은 예제 프로젝트).
- **이름 바꾸기 = exe 이름 바꾸기**: 확인 창 뒤 `tools\rename_project.ps1`을 창 없이 실행한다. `mealkit.project`의 `name` 줄(원래 형식), 템플릿 파일
  (고치지 않았으면 새 이름으로 다시 채우고 `.mealkit-template` 해시도 새로, 고쳤으면 옛 이름만 바꾸고 "check it" 표시), `source\`의 문자열·주석 안 이름(코드 식별자는 그대로)을 바꾸고 `build\`를 지운다.
  `export\<옛 이름>`은 남긴다.
- **폴더 옮기기**: 확인 창 뒤 `build\`(절대 경로가 들어 있다)를 지우고 같은 드라이브면 이름 바꾸기, 아니면 복사 후 지우기. 목록·즐겨찾기 경로를 바꾼다. 옮기기 전에 시작일을 `mealkit.project`에 적는다(폴더 생성 시각이 바뀔 수 있어서).
  엔진 저장소의 예제 프로젝트는 이름·폴더를 바꿀 수 없다.
- **아이콘**: 프로젝트 설정 `icon`(기본 `icon.png`, 프로젝트 폴더 기준). 관리자에서 그림(PNG·JPEG·BMP·ICO 등)을 고르면 256px 이하 PNG로 복사한다. 없으면 밀키트 아이콘(`MealKitEditor.exe`의 RCDATA 2번).
  그림 읽기·쓰기는 WIC(`editor/Image.cpp`), 화면에는 ImGui 사용자 텍스처(`ImGuiLayer::createTexture`/`releaseTexture`). 정사각형이 아니면 가운데를 잘라 보인다.
- **날짜**: 프로젝트 설정 `created`(읽기 전용, `new_project.ps1`이 `@CREATED@`로 적는다. 없으면 폴더 생성 시각). 수정일은 `build`·`export`·`third_party`·점으로 시작하는 폴더를 뺀 가장 최근 파일 시각.
- **사용자 파일** (`%LOCALAPPDATA%\MealKit`): `recent_projects.txt`(최대 100개), `favorite_projects.txt`, `editor_settings.txt`(`language ko`). BOM이 붙어 있어도 읽는다.
- **에디터 글자 번역** (`editor/Strings.h`): 코드에는 영어 원문을 `tr()`/`trl()`/`trf()`로 쓰고 한국어는 `engine/res/strings/ko.txt`(247줄). 기본 언어 한국어, 번역이 없으면 영어.
  `trl()`은 `번역###원문`이라 언어를 바꿔도 도킹·팝업 ID가 같다(기존 `###Hierarchy` 같은 ID는 그대로라 레이아웃 파일도 이어진다).
  엔진이 선언한 필드 설명·씬 기본 종류 이름은 영어로 바꾸고 `trData()`로 번역한다. 게임이 선언한 글자는 게임의 언어 그대로. 되감기 오류 메시지(`Simulation.h`)는 영어.
  `tools\check_strings.ps1`(check.ps1 항목)이 빠진 번역·안 쓰는 줄·printf 형식 차이·에디터 코드의 한글 문자열을 찾는다.
- **글꼴**: Segoe UI(영문) + 맑은 고딕(한글) + Segoe Fluent Icons/MDL2 Assets(아이콘 글자 `editor::icon::`)를 합친다. 맑은 고딕만 쓸 때 역슬래시가 ₩로 보이던 것이 고쳐졌다.
- **템플릿**: `mealkit.project`에 `created` 줄. 템플릿이 바뀌어 엔진 버전 0.4.1 → 0.4.2.
- **검증**: 관리자·에디터 화면을 한국어/영어로 캡처해 확인. 임시 프로젝트로 `rename_project.ps1`을 두 번 돌리고 게임 DLL(`<새 이름>_game.dll`)이 빌드되는 것을 확인. `check.ps1` 통과.
  관리자 안에서 클릭으로 하는 이름 고치기·폴더 옮기기·아이콘 고르기는 직접 눌러 보지 않았다.
- **게임 exe 아이콘**: `mealkit_add_game()`(`cmake/MealKit.cmake`)이 구성 때 `mealkit.project`의 `icon`(빈 값이면 `icon.png`, 프로젝트 폴더 기준, 절대 경로도 됨)을 읽고,
  빌드 때 `cmake/MakeIcon.ps1`(PowerShell + System.Drawing, 외부 도구 없음)이 16·20·24·32·40·48·64·128·256px PNG 항목의 `.ico`로 바꿔 `<이름>_icon.rc`로 아이콘 리소스 1번에 넣는다.
  정사각형이 아니면 비율을 지켜 가운데에 놓는다. 그림이 없으면 엔진 기본 아이콘(`engine/res/MealKitEditor.ico`). Release·Debug·Profile·Export 모두 같고 Editor 빌드(게임 DLL)는 해당 없다.
  `mealkit.project`는 구성 의존, 그림 파일은 `CONFIGURE_DEPENDS` GLOB으로 지켜봐서 그림을 새로 고르거나 지워도 `build.bat`만 다시 하면 된다. exe가 약 33 KB 커진다(sandbox 1.58 → 1.62 MB).
  템플릿은 그대로라 엔진 버전은 0.4.2 그대로. 검증: sandbox(기본 아이콘·임시 `icon.png`)와 새 프로젝트(`icon art\logo.png`)의 Release·Export exe에서 셸 아이콘을 꺼내 확인.
- **남은 것**: 다른 언어 추가는 `kLanguages`와 `<언어>.txt`만 더하면 된다.

### 에디터 창 모양 (2026-10-16)
- **제목 표시줄**: `WindowDesc::captionColor`(0xRRGGBB)를 주면 DWM 어두운 제목 표시줄(Windows 10 20H1+)과 제목 표시줄 색(Windows 11)을 켠다.
  에디터·프로젝트 관리자·참가 창은 `editor::kEditorCaptionColor`(ImGui 메뉴 막대 색)를 쓴다. 게임 창은 기본값(시스템 색) 그대로.
- **아이콘**: `Window`가 exe의 아이콘 리소스 1번을 창 아이콘으로 쓴다(없으면 Windows 기본). `MealKitEditor.exe`에 `engine/res/MealKitEditor.rc`/`.ico`를 넣었다.
  아이콘은 `python tools/make_editor_icon.py`(Pillow)로 다시 만든다. 게임 exe 아이콘은 v0.4.2 프로젝트 관리자 항목에서 넣었다.

## v0.4.1 — 1단계 진행 중 (2026-10-16)

### [x] 프로젝트 설정 (`mealkit.project`를 필드 선언으로) (완료 2026-10-16)
- **구현됨**: 엔진의 `MEALKIT_STRUCT(game::ProjectSettings)`(`mealkit/game/ProjectSettings.h`: `name`, `engine`, `engineVersion`, `scene`)와
  `game::ProjectFile`(설정 + `game.*` 줄 + 모르는 줄 + 머리 주석). `mealkit.project`는 `.tuning`과 같은 "경로 값" 줄로 쓰고, 예전 "키=값" 줄도 읽는다.
  모르는 키는 경고하고 저장할 때 그대로 다시 쓴다(새 엔진이 쓴 항목을 잃지 않는다).
- **게임 항목**: 게임이 `GameClass::ProjectSettingsType`(MEALKIT_STRUCT)을 선언하면 `MEALKIT_GAME_MODULE`이 `GameModule::projectSettingsType`으로 넘긴다(없어도 된다).
  `kApiVersion` 6 → 7. 파일에서는 `game.` 접두사. 에디터는 게임 DLL을 바꿔 끼울 때 튜닝처럼 값을 텍스트로 보존한다.
  예제(sandbox)는 `app/ProjectSettings.h`에 `defaultPort`(27015), `defaultMaxPlayers`(4)를 옮겼다(전에는 `main.cpp` 상수).
- **문자열 필드**: 필드 선언에 `std::string`(`reflect::Kind::String`)을 더했다. 텍스트는 줄 끝까지(앞뒤 공백 없음), 해시는 길이 + 바이트, 스냅샷 저장·복원, 차이 보고, 에디터 입력 칸.
  시뮬레이션 상태에는 넣을 수 없다: `game::Simulation`이 `reflect::hasStringField<State>()`로 컴파일 때 막는다.
  `kFieldReadOnly` 플래그와 `MEALKIT_FIELD_FLAGS`를 더했다(에디터가 회색으로 보여 주고 고치지 않는다: 이름·엔진 폴더·엔진 버전은 도구가 관리).
- **에디터**: [파일] > [프로젝트 설정...] 창(엔진 항목 + 게임 항목, 저장·되돌리기, "지금 씬을 시작 씬으로"). `Ctrl+S`와 출시 빌드 전에 함께 저장한다.
  창 제목에 "(프로젝트 설정 *)". 업그레이드가 끝나면 도구가 고친 `engine`·`engineVersion`만 다시 읽는다. 자동 테스트용 `--project-settings`.
- **게임 실행 파일**: `game::loadGameProject()`가 실행 파일 폴더(출시본)나 그 위(개발 중)의 `mealkit.project`를 찾아 읽는다. 시작 씬이 `main.cpp`와 파일 두 곳에 있던 것이 파일 하나로 줄었다.
  출시 빌드(`MealKitExport.cmake`)가 `mealkit.project`를 exe 옆에 넣는다(개발 PC 경로가 든 `engine`·`engineVersion` 줄은 뺀다).
- **콘텐츠 해시**: `game::contentHash`가 엔진·게임 프로젝트 설정을 `kFieldView`를 빼고 넣는다. 엔진 공통 항목은 지금 모두 해시 제외(틱레이트 같은 시뮬레이션 항목이 생기면 해시 대상으로 선언한다).
  에디터 여러 명 플레이는 디스크에 저장된 값으로 해시를 만든다(참가 창과 같은 값).
- **도구·템플릿**: 템플릿 `mealkit.project`가 새 형식. `build.bat`·`editor.bat`은 `delims== `로 두 형식을 다 읽는다. `project_common.ps1`도 두 형식을 읽고,
  `upgrade_project.ps1`은 `engine`·`engineVersion` 줄을 **원래 형식 그대로** 고친다(고친 `build.bat`이 예전 형식만 읽을 수 있어서). 템플릿이 바뀌어 엔진 버전 0.4.0 → 0.4.1.
- **검증**: 단위 테스트 `reflect_string`, `project_file`(두 형식 섞인 파일 읽기, 저장 후 다시 읽기, 해시 제외). `check.ps1` 통과, `mp_test` PASS.
- **남은 것**: 씬 목록(문자열 목록 필드가 필요하다. 씬 여러 개 다루기·팩 파일 때). 이름 바꾸기(CMake 타깃·exe 이름이 따라 바뀌어야 해서 지금은 읽기 전용).
  예전 형식 `mealkit.project`를 에디터가 저장하면 새 형식이 되므로, `build.bat`을 직접 고쳐 업그레이드가 `.new`로 남긴 프로젝트는 그 `build.bat`의 `delims`를 고쳐야 한다.

## v0.4 — 0단계: 개발자 경험 (2026-10-15)

기능을 더 쌓기 전에 툴부터 편하게 만든다. 툴이 불편한 채로 기능을 쌓으면 나중에 그 기능들을 다시 고쳐야 한다.
원칙: **무엇을 어디에, 얼마나는 에디터(데이터)에서, 어떻게 동작하는지는 코드에서** 다룬다. 비주얼 스크립팅은 만들지 않는다.

### [x] 고도식 범용 에디터 + 게임 DLL 핫 리로드 (결정·완료 2026-10-15)
- **구현됨**: `MealKitEditor.exe` + `mealkit.dll`(Editor 빌드, RelWithDebInfo, 동적 CRT), 프로젝트 관리자(최근 목록, 폴더 열기, 새 프로젝트),
  `mealkit.project`, `<이름>_game.dll`(게임 코드만, 다시 빌드 수 초), 원본 DLL 감시 핫 리로드, `Ctrl+R` 게임 코드 빌드, 빌드 실패 시 이전 DLL 유지,
  엔진이 더 새것이면 열 때 자동 빌드, `GameInstance` API 버전 검사. 출시 exe는 그대로(결정론 해시 동일 확인).
- **남은 것**: 디버거가 붙어 있으면 PDB가 잠겨 핫 리로드 빌드가 실패할 수 있다(PDB 이름을 빌드마다 바꾸는 방식 검토).
  mealkit.dll을 다시 빌드할 때는 에디터를 닫아야 한다. 디버그 CRT용 에디터 구성(EditorDebug)은 아직 없다.
- **왜**: 지금은 프로젝트마다 에디터 exe가 따로 있고 게임 코드가 그 안에 들어 있어서, 코드를 고치면 에디터를 닫고 빌드하고 다시 열어야 한다. 에디터와 VS Code를 같이 띄워 놓고 일할 수 없다.
- **구조**:
  - 엔진 저장소가 **에디터 하나**(`MealKitEditor.exe`)를 만든다. 실행하면 **프로젝트 관리자**(최근 프로젝트, 열기, 새 프로젝트)가 뜨고, 프로젝트 폴더를 연다.
  - 프로젝트는 에디터용으로 **게임 DLL**(`<이름>_game.dll`: World, GameSession)만 빌드한다. 에디터가 이 DLL을 불러 플레이 모드를 돌린다.
  - VS Code에서 DLL만 다시 빌드하면 에디터가 편집 모드에서 바꿔 끼운다(씬·카메라·선택 유지). 컴파일 에러는 에디터 로그 창에도 표시.
  - **출시용 `<이름>.exe`는 지금처럼 정적 단일 exe** (가벼움, 결정론 영향 없음).
- **설계 결정할 것**:
  - 에디터와 게임 DLL이 엔진을 같이 써야 하므로(`PhysicsWorld`, `ModelCache` 등), 에디터 구성에서는 엔진도 DLL(`mealkit.dll`)로 만들고 CRT는 동적(`/MD`)으로 쓴다. 출시 구성은 정적 그대로.
  - 게임 DLL 진입점은 C 함수 하나(`mealkitGameCreate`)와 **API 버전 번호**. 엔진과 DLL 버전이 다르면 불러오지 않고 다시 빌드하라고 알린다(크래시 대신).
  - 에디터 플레이와 출시본이 네트워크로 섞이지 않도록 콘텐츠 해시에 빌드 구성을 포함한다.
- **Dev 빌드 구성**: 최적화는 켜고 링크 시간 최적화(LTCG)는 끈 구성. 게임 DLL 다시 빌드를 수 초로 줄인다.
- **영향**: `new_project.ps1`, 프로젝트 템플릿, `mealkit_add_game()`, 에디터 [빌드] 메뉴(프로젝트 경로를 실행 중에 알게 됨)가 바뀐다. 프로젝트의 `editor.bat`은 엔진 에디터에 프로젝트 경로를 넘기는 역할만 남는다.
- **단점(감수)**: DLL 경계 관리, 엔진·DLL 버전 불일치 처리, 디버깅 설정이 조금 복잡해진다.

### [x] 게임 코드 구조: `sim/` · `view/` 폴더와 결정론 검사 (완료 2026-10-15)
- **구현됨**: 예제·템플릿 코드를 `sim/`(World: 상태·`step` 순서·`hash`, `Systems.h`, `Players.cpp`, `Props.cpp`), `view/`(Camera, LocalInput, Draw), `app/`(GameSession, main, GameModule)로 나눴다.
  `World::step()`은 시스템 함수를 순서대로 직접 부르고, 시스템 함수는 `World&`만 받는다(이번 틱 입력은 `world.inputs`). 상자 색은 sim에서 팔레트 번호만 갖고 색은 `view/`가 정한다.
  구조를 바꾼 뒤에도 같은 시드·입력의 상태 해시가 이전과 같다(`mp_test` PASS).
- **검사**: `engine/tools/SimCheck.cpp`(`mealkit_simcheck`)가 `sim/` 파일을 토큰 단위로 검사한다(주석·문자열 제외). `mealkit_add_game()`이 게임 타깃보다 먼저 실행하고,
  어기면 `파일(줄,열): error SIMnnn`과 대신 쓸 것을 출력하고 빌드를 멈춘다(VS Code `$msCompile`이 잡는다). 게임 프로젝트의 Editor 빌드는 `MealKitTargets.cmake`로 이 도구를 가져온다.
  막는 것: CRT 수학 함수(호출), `quatFromAxisAngle`, `rand`·`<random>`, 실제 시간(`<chrono>`, `time()`, `nowSeconds`), `new`/`delete`/스마트 포인터,
  `virtual`/`override`, 코루틴·스레드·`atomic`, `unordered_*`, 가변 `static`·전역 변수·`thread_local`, `using namespace std`, 렌더러·플랫폼·에디터·`game/`·`view/`·`app/` 헤더.
- **CMake**: `mealkit_add_game(<이름> [SOURCE_DIR source])`이 `sim/ view/ app/`을 `GLOB_RECURSE CONFIGURE_DEPENDS`로 모은다. 예전 방식(`SOURCES` 나열)도 그대로 받는다(이때 `sim/` 폴더가 없으면 검사도 없다).
- **남은 것**: 설정 헤더는 설정 시스템(1단계)이 생기면 금지 목록에 넣는다. 매크로 안에 숨긴 금지 코드와 함수 포인터로 넘긴 수학 함수는 잡지 못한다(그림자 시뮬레이션·`mp_test`가 마지막 확인).
  이미 만든 프로젝트는 자동으로 옮겨지지 않는다(프로젝트 업그레이드 도구 항목).

### [x] 기본 도구: `Pool<T>` · 틱 이벤트 · 틱 타이머 · `GameContext` (완료 2026-10-15)
- **구현됨**: `mealkit/sim/Pool.h`(`Id<T>` 세대 번호, `for (auto [id, item] : pool)` 슬롯 순서 순회, `endTick(onDestroy)`, `hashState`),
  `mealkit/sim/TickEvents.h`, `mealkit/sim/TickTimer.h`, `mealkit/game/GameContext.h`(창·렌더러·에셋·세션 + 뷰포트·입력 허용, `kApiVersion` 2).
  `MEALKIT_ASSERT`는 Editor 빌드에서도 켜진다(`MEALKIT_ASSERTS`, 디버거가 없으면 로그만). sandbox의 상자는 `Pool<Prop>`(바디 id를 항목에 둔다),
  밀치기 쿨다운은 `TickTimer`, 밀치기는 틱 이벤트 → 화면 충격파(`view/Effects`). SimCheck 안내 문구가 이 도구들을 가리킨다.
- **결과**: 상태 해시에 Pool 슬롯 상태가 들어가 해시 값 자체는 이전과 다르다(`mp_test` 3+1 피어 900틱 PASS). Pool 용량은 튜닝 시스템 전까지 `World::reset` 인자.
- **남은 것**: sandbox는 상자를 지우지 않아 삭제 경로는 단위 테스트로만 확인했다(실제 게임 코드에서 처음 쓸 때 `mp_test`로 다시 확인).
  이미 만든 프로젝트(`D:\GameEngine\MealKit\Sandbox`)는 옛 `GameInstance` 시그니처라, 새 엔진으로는 게임 코드 빌드가 실패한다(에디터에서 플레이 불가). 손으로 옮기거나 업그레이드 도구로 처리한다.
- **왜**: `docs/CODING_MODEL.md`의 "한 문제에 한 답"은 엔진이 그 답에 쓸 도구를 줘야 지켜진다. 도구가 없으면 프로젝트마다 다시 만들고 모양이 갈린다.
- **방식**: `mealkit/sim/`에 `Pool<T>`(매치 시작 때 정하는 고정 용량, 세대 번호 id, 즉시 생성·틱 끝 일괄 삭제, 만든 틱에는 순회 제외),
  시뮬레이션 → 화면 전용 틱 이벤트 배열, 틱 타이머(`readyTick`) 헬퍼. 화면 쪽은 엔진 서비스(렌더러·입력·소리·에셋·UI·디버그 UI)를 묶은 `GameContext&` 하나를 `GameInstance`에 넘긴다(`kApiVersion` 올림).
  `GameSession`은 `step()`마다 틱 이벤트를 화면 쪽 큐로 옮긴다(한 프레임에 여러 틱이 돌 때 빠지지 않게). `MEALKIT_ASSERT`를 Editor 빌드에서도 켠다.
- **순서**: 오브젝트 타입 시스템과 상태 필드 선언이 이 도구 위에 놓이므로 그보다 먼저 한다.
- **결정론**: Pool 순회는 슬롯 순서. Pool 상태(세대 번호, 빈 슬롯 목록 포함)는 `hash()`에. 틱 이벤트는 시뮬레이션이 읽지 않으므로 해시에 넣지 않는다.

### [x] 게임 정의 오브젝트 타입 + 인스펙터 필드 (완료 2026-10-15)
- **구현됨**: 필드 선언 장치 `mealkit/core/Reflect.h`(`MEALKIT_STRUCT`/`MEALKIT_OBJECT`/`MEALKIT_FIELD`/`MEALKIT_VIEW_FIELD`/`MEALKIT_ARRAY`).
  `__COUNTER__`로 필드 번호를 매기고 실행 중 설명(`reflect::TypeInfo`)을 만들어, 타입을 모르는 엔진·에디터가 텍스트 저장, 해시, 스냅샷, 비교, 위젯을 처리한다.
  씬 물체는 `type` 이름(기본 `solid`/`decoration`/`spawn` + 게임 종류)과 필드 텍스트(`field <경로> <값>`)를 가진다(씬 파일 version 2, version 1도 읽는다).
  게임 코드가 없거나 필드가 바뀌어도 값은 보존된다. 게임 DLL은 `GameModule::objectTypes`로 종류를 넘기고(`kApiVersion` 3),
  에디터는 [추가] 메뉴·`+ 종류...`·인스펙터 "종류" 콤보와 필드 위젯(`editor/FieldEditor`)을 보여 준다. 게임은 `scene::readObject<T>(entity)`로 읽는다.
  예제: sandbox `JumpPad`(점프 힘, 재사용 대기) — 아레나에 하나 놓았다.
- **콘텐츠 해시**: 장식 종류(`kObjectCosmetic`)와 `MEALKIT_VIEW_FIELD`는 뺀다. 게임은 `scene.contentHash(&reflect::moduleObjectTypes())`.
- **왜**: 지금 에디터에 놓을 수 있는 물체 종류(`Solid`/`Decoration`/`SpawnPoint`)가 엔진 `Scene.h`에 박혀 있다. 코인·트리거·점프대를 만들려면 엔진을 고쳐야 한다.
- **방식**: 게임 코드가 타입과 필드를 한 번 선언하면 에디터의 `추가` 메뉴와 인스펙터에 자동으로 나타나고 `.scene`에 저장된다. `World`는 매치를 시작할 때 읽는다.
  ```cpp
  MEALKIT_OBJECT(JumpPad) {
      MEALKIT_FIELD(f32, power, 12.0f, "점프 힘");
      MEALKIT_FIELD(u32, cooldownTicks, 30, "재사용 대기(틱)");
  };
  ```
- **기존 종류**: 상자·장식·스폰도 이 시스템 위의 기본 타입으로 옮긴다. 기존 `.scene`은 그대로 읽히게 한다.
- **결정론**: 필드 값은 콘텐츠 해시에 포함한다(장식용 필드는 표시해서 제외). 순회는 오브젝트 id 순서.

### [x] 튜닝 값을 데이터로 (완료 2026-10-15)
- **구현됨**: 게임이 `MEALKIT_STRUCT(Tuning)`(sandbox `sim/Tuning.h`)을 선언하고 `GameSession::TuningType`으로 알린다(`GameModule::tuningType`, `kApiVersion` 4).
  값은 `assets/game.tuning`(텍스트 `경로 값`). `game::loadTuning`이 읽고 `World::reset`이 `world.tuning`으로 복사한다. 콘텐츠 해시는 `game::contentHash`(씬 + 모델 + 튜닝).
  에디터 [튜닝] 패널(환경 옆 탭)에서 고치고 저장(Ctrl+S도 같이)한다. 플레이를 누르면 저장 안 된 값을 `build\Editor\play\play.tuning`으로 넘기고,
  플레이 중에 고치면 `GameInstance::setTuning`으로 바로 적용한다(혼자 플레이). 게임 실행 파일은 `--tuning <파일>`로 다른 값을 쓸 수 있다.
  sandbox의 이동·점프·중력·밀치기·상자 밀도 상수를 튜닝으로 옮겼다(같은 값이라 상태 해시도 이전과 같다).
- **왜**: 이동 속도·점프력·쿨다운이 `World.cpp`의 `constexpr`이라 값 하나 맞추는 데도 빌드가 필요하다.
- **방식**: 위와 같은 필드 선언으로 게임 전역 설정(`assets/game.tuning` 같은 텍스트 파일)을 만들고 에디터 패널에서 고친다.
  에디터 혼자 플레이에서는 값을 바꾸면 바로 적용, 멀티플레이에서는 매치 시작 때 고정하고 콘텐츠 해시에 넣는다.

### [x] 상태 필드를 한 번만 선언 (완료 2026-10-15)
- **구현됨**: sandbox `WorldState`·`Player`·`Prop`·`Pad`를 `MEALKIT_STRUCT`/`MEALKIT_FIELD`로 선언했다. `World::hash()`는 `reflect::hashStruct`(선언한 필드 전부) + 물리 해시 한 줄이다.
  스냅샷: `game::saveSnapshot`/`loadSnapshot`(`mealkit/game/WorldSnapshot.h`, 상태 필드 + `PhysicsWorld::save/restore`).
  물리 스냅샷은 바디 목록·모양·Jolt 상태(`SaveState`)를 담고, 복원하면 그 사이에 만들거나 지운 바디도 되돌린다. 바디 id는 엔진이 정하는 순서로 매겨(`CreateBodyWithID`) 복원 뒤에 만든 바디도 같은 id가 된다.
  단위 테스트 `physics_snapshot`: 복원 후 40틱 동안 해시가 처음과 같다(중간에 바디 삭제·생성 포함).
  에디터 [상태] 패널(인스펙터 옆 탭, 플레이하면 앞으로)이 선언을 그대로 읽기 전용 표로 보여 준다(`GameInstance::stateView`, `kApiVersion` 5).
- **바뀐 것**: 바디 id 할당 방식이 바뀌어 상태 해시 값이 이전과 다르다(피어끼리는 같다, `mp_test` PASS).
- **왜**: 새 상태 하나에 구조체, `step`, `hash`, 그리기를 고쳐야 하고, `hash`를 빠뜨려도 조용히 디싱크 감지에서 빠진다. 중간 참가(스냅샷)를 넣으면 한 곳이 더 는다.
- **방식**: 시뮬레이션 상태도 필드 선언(위와 같은 장치)으로 만들고 **해시, 스냅샷 저장·복원, 에디터 실시간 보기**를 자동 생성한다. 3단계 "중간 참가"의 기반.

### [x] 결정론 수학 유틸 (삼각함수 근사) (완료 2026-10-15)
- **구현됨**: `mealkit/core/DetMath.h`(`detSin`/`detCos`/`detAtan`/`detAtan2`/`detQuatFromAxisAngle`/`detYawQuat`, Cephes 계수). CRT와의 최대 오차 sin·cos 6e-8, atan2 2.4e-7.
  엔진 단위 테스트 `mealkit_tests`(엔진 저장소 빌드)가 오차와 결과 비트 해시(`detmath-bits`)를 출력하고, Release·Editor 빌드에서 같은 해시를 확인했다. SimCheck SIM001 안내가 이 함수들을 가리킨다.
- **왜**: 시뮬레이션에서는 CRT `sin`/`cos`/`atan2`를 쓸 수 없는데, 엔진에 대신 쓸 함수가 없다(`quatFromAxisAngle`도 시뮬레이션 금지). 캐릭터 회전, AI 방향 전환, 래그돌 목표 포즈 모두 각도 계산이 필요하다.
- **방식**: `mealkit/core/DetMath.h`에 사칙연산과 `sqrt`만 쓰는 다항식 근사 `detSin`/`detCos`/`detAtan2`/`detQuatFromAxisAngle`. 입력 범위를 정수 연산으로 줄인 뒤(각도 → [-π/4, π/4]) 근사한다.
- **검사**: 모든 입력 구간에서 CRT 결과와의 오차 한계를 테스트하고, 결과 비트가 Release/Editor 구성에서 같은지 `mp_test`·해시로 확인한다. `sim/` 금지 헤더 검사는 CRT 함수 대신 이 함수를 쓰라고 안내한다.
- **비용**: 작음. 아래 "플레이어·캐릭터 분리"와 4단계 "이동 모드"의 전제.

### [x] 플레이어와 캐릭터 분리 (빙의) (완료 2026-10-15)
- **구현됨**: sandbox `WorldState`가 `slots`(PlayerSlot: 접속, 팀, 떨어진 횟수, 컨트롤러), `controllers`(Pool<Controller>: 플레이어/AI, 빙의한 Pawn, 리스폰 타이머, AI 목표),
  `pawns`(Pool<Pawn>: 몸, 물리 바디)로 나뉜다. `possess`/`unpossess`(sim/Pawns.cpp)로만 잇는다.
  떨어진 Pawn은 없애고(바디는 틱 끝에 삭제) 컨트롤러는 `respawnTicks` 뒤 새 Pawn에 빙의한다. 그동안 카메라는 마지막 자리에 머문다(관전).
  AI 컨트롤러(튜닝 `aiPawns`, 기본 2)는 시뮬레이션 안에서 `aiRng`(Pcg32, 상태 필드)와 `detSin`/`detCos`로 목표를 골라 걷는다(대역폭 0).
  `--bot`은 입력을 만드는 테스트 도구로 그대로 둔다. `mp_test.ps1`에 `-Scene`, `-Tuning`을 추가했다(바닥 없는 씬으로 리스폰 반복 시험 PASS).
- **왜**: 지금은 `players[i]`가 접속한 사람이면서 동시에 캡슐 몸이다. 리스폰 대기, 관전, 차량 탑승, AI 캐릭터, 죽어도 남는 점수를 넣을 자리가 없다.
- **방식**: 시뮬레이션 상태를 세 가지로 나눈다.
  - `PlayerSlot`: 매치 동안 유지. 접속 상태, 이름, 팀, 점수, 이번 틱 입력.
  - `Pawn`: 월드 안의 몸(종류, 이동 모드, 체력 등). 생기고 사라지며 id 순서로 갱신.
  - `Controller`: 슬롯 또는 AI를 Pawn에 잇는다. 플레이어 / AI / 없음(관전). `possess(controller, pawnId)`, `unpossess(controller)`.
- **언리얼과 다른 점**: Actor 상속 트리는 만들지 않는다. 전부 시뮬레이션 안의 평평한 배열이고, 빙의 변경도 시뮬레이션 안에서만 일어난다(클라이언트는 입력으로 "탑승 요청"만 보낸다).
- **AI**: AI 컨트롤러는 시뮬레이션 안에서 결정론으로 돈다(모든 피어가 같은 계산, 대역폭 0). 지금의 `--bot`(입력을 만들어 보내는 방식)은 테스트 도구로 따로 둔다.
- **템플릿**: `sim/`·`view/` 구조와 같이 템플릿에 넣는다. 나중에 넣으면 만든 프로젝트를 모두 옮겨야 한다.
- **결정론**: 슬롯·Pawn·Controller 상태 모두 `hash()`에. Pawn 생성·삭제는 틱 안에서 정해진 순서로, id는 재사용하지 않거나 세대 번호를 붙인다.
- **이어지는 것**: 3단계 중간 참가(빈 슬롯 이어받기), 4단계 이동 모드·게임 흐름·체력 모듈.

### [x] 씬 트리 계층 (부모·자식) (완료 2026-10-15)
- **구현됨**: `scene::Entity::parent`(씬 파일 `parent <id>`), 위치·회전·크기는 부모 기준. `worldTransform`/`setWorldTransform`/`isDescendant`/`subtree`/`flattened`(Scene.h).
  에디터 씬 트리는 접고 펴는 트리이고, 끌어다 놓으면 부모가 바뀐다(화면 위치 유지, 자기 자손 아래로는 막음). 빈 곳에 놓거나 우클릭 "최상위로 꺼내기"로 빼낸다.
  기즈모는 월드 공간, 인스펙터는 부모 기준 값. 복제·삭제는 자식까지 함께. 게임은 `scene::flattened()`로 펼친 평평한 목록을 쓴다(sandbox GameSession::start).
  없는 부모나 순환은 불러올 때 최상위로 옮기고 경고한다. 크기는 축별 곱(회전된 부모의 비균등 크기에서 생기는 찌그러짐은 버림).
- **왜**: 여러 물체를 묶어 옮기기, 움직이는 발판 위의 장식 같은 경우 부모·자식이 필요하다.
- **방식**: `Entity`에 `parent` id를 두고 에디터에서는 트리로 보여 준다(끌어서 부모 변경). **게임이 불러올 때 월드 좌표로 펼쳐** 실행 중에는 지금처럼 평평한 배열로 둔다(속도·결정론 유지). 4단계 "프리팹"과 이어진다.

### [x] 에디터에서 여러 명으로 플레이 (완료 2026-10-15)
- **구현됨**: 플레이 버튼 옆 인원(혼자/2~4명)과 "참가 창 봇". 여러 명이면 에디터가 방장(127.0.0.1:27215)이 되고, 같은 에디터 실행 파일을 참가 창 모드
  (`MealKitEditor.exe --project <폴더> --player <주소> --scene <파일> --tuning <파일> [--bot]`)로 더 띄운다. 참가 창은 현재 씬·튜닝(저장 안 된 값 포함)을
  `build\Editor\play\`의 임시 파일로 받아 같은 게임 DLL로 접속하고, 콘텐츠 해시(`editor::playContentHash`)가 같아야 들어온다.
  참가 창은 Job 개체(`ProcessGroup`)로 묶어 정지하거나 에디터가 꺼지면 같이 닫힌다. 로그는 `build\Editor\play\player<N>.log`.
  상태 표시줄에 인원·동기화 여부. 30초 안에 모두 접속하지 않으면 알림을 띄우고 멈춘다. 여러 명일 때 튜닝은 바로 적용하지 않는다(디싱크 방지).
  게임 DLL 핫 복사본 이름에 프로세스 번호를 넣어 여러 프로세스가 같이 쓸 수 있다. 자동 테스트: `--play --players 3 --bot`.
  플레이 진행은 `editor/PlaySession`으로 옮겼다.
- **왜**: 멀티플레이 엔진인데 여러 명 테스트는 명령줄로 창을 직접 띄워야 한다.
- **방식**: 플레이 버튼 옆에서 "플레이어 2~4명"을 고르면 현재 씬(저장 안 된 편집 포함, 임시 파일로)으로 `--host --local` 창 1개 + 참가 창을 자동으로 띄운다.
  추가 창은 봇 입력 또는 직접 조작. 정지하면 모두 닫는다. 유니티 Multiplayer Play Mode에 해당.

### [x] 그림자 시뮬레이션으로 디싱크 즉시 감지 (완료 2026-10-15)
- **구현됨**: 엔진 `game::Simulation<World>`(mealkit/game/Simulation.h)가 고정 틱 실행·보간용 직전 상태·그림자 World를 맡는다(게임의 GameSession은 이것 하나를 갖는다).
  `PlayOptions::shadow`이면 같은 입력으로 World를 하나 더(물리 스레드 1개) 돌려 매 틱 해시를 비교하고, 다르면 그 틱에서 멈추고
  `reflect::diffStruct`로 다른 상태 필드를 남긴다(비어 있으면 물리만 다름). 에디터는 메뉴 막대에 "그림자: 일치/갈라짐"을 보이고 알림 창으로 차이를 보여 준다.
  에디터 메뉴 막대의 "그림자 검사"(기본 켬)로 끈다. 게임 실행 파일은 `--shadow`(결과 `SHADOW ok|DESYNC`).
  단위 테스트: 전역 변수를 섞은 가짜 World에서 첫 틱에 잡고 `sum` 필드 차이를 보고한다. sandbox 900틱(바디 2000개) 일치.
- **왜**: 결정론 실수는 지금 `mp_test`를 돌려야 드러나고, 어디서 났는지 찾기 어렵다.
- **방식**: 에디터 플레이 중 같은 입력으로 `World`를 하나 더 돌리고 매 틱 해시를 비교한다. 다르면 즉시 멈추고 틱·상태 차이를 보여 준다.
- **잡는 것 / 못 잡는 것**: 초기화 안 된 변수, 포인터 주소 순서, 실제 시간 읽기, 전역 상태는 잡는다. CPU마다 다른 계산(FMA 등)은 못 잡는다(`mp_test`, 다른 PC 테스트로).
- **비용**: 시뮬레이션 CPU 2배 (에디터에서만, 끌 수 있게).

### [x] 일시정지, 한 틱씩 진행, 되감기 (완료 2026-10-15)
- **구현됨**: `PlayOptions::record`(에디터 혼자 플레이)이면 틱 입력·해시를 기록하고 150틱마다 `WorldSnapshot`(상태 필드 + 물리)을 남긴다(최근 40개, 약 3분 20초).
  `GameInstance::debug()`(`game::SimDebug`, `kApiVersion` 6): 일시정지(F6), 한 틱(F7), 타임라인 슬라이더로 되감기(가까운 스냅샷에서 기록된 입력으로 다시 돌림).
  다시 돌린 해시가 기록과 다르면 "선언하지 않은 상태 또는 결정론 위반"으로 알린다. 계속하면 현재 틱으로 돌아간다. 튜닝을 바꾸면 기록을 새로 시작한다.
  여러 명 플레이에서는 멈출 수 없으므로 기록하지 않는다. 게임 실행 파일 `--rewind-test`(결과 `REWIND ok|FAIL`): sandbox 900틱 기록 3.8 MB, 전 구간 재현 일치.
- **왜**: 버그가 난 순간을 다시 보기 어렵다. 결정론이라 입력만 기록하면 어느 틱으로든 돌아갈 수 있다.
- **방식**: 에디터 플레이 중 입력을 기록하고 일정 간격(예: 5초)마다 스냅샷을 남긴다. 타임라인 슬라이더로 가까운 스냅샷에서 빨리 감아 원하는 틱으로 간다. 일시정지 중에는 상태 인스펙터로 값을 본다.
- **의존**: "상태 필드를 한 번만 선언"의 스냅샷 기능.

### [x] 주기적 자동 저장 (완료 2026-10-15)
- **구현됨**: 저장 안 된 변경이 있으면 편집 중 90초마다 `<씬>.autosave`에 쓴다(원본은 그대로). 저장하면 자동 저장본을 지운다.
  씬을 열 때 자동 저장본이 더 새로우면 "복구 / 버리기"를 묻는다(복구도 되돌리기 가능). 에러 알림용 모달(`Editor::showMessage`)도 추가했다.
- **왜**: 지금은 에디터를 정상 종료할 때만 `.autosave`를 남긴다. 크래시가 나면 그 사이 작업이 사라진다.
- **방식**: 변경이 있으면 1~2분마다 `.autosave`에 쓰고, 다음 실행 때 복구할지 묻는다. 원본 파일은 덮어쓰지 않는다.

### [x] 한 번에 도는 검증 스크립트 (`tools/check.ps1`) (완료 2026-10-15)
- **구현됨**: 15개 항목을 차례로 돌리고 PASS/FAIL과 걸린 시간을 출력한다(실패가 있으면 종료 코드 1). 로그·스크린샷은 `build\check\`.
  Editor·Release 빌드, 단위 테스트 두 빌드(결정론 수학 비트 일치), `mp_test` 1+3, 그림자·되감기(`--shadow --rewind-test`), 게임 캡처, 에디터 캡처,
  에디터 혼자 플레이(그림자 검사), 에디터 3명 플레이, SimCheck가 잘못된 sim/ 코드를 막는지(임시 파일로 SIM002), 빌드 실패 때 이전 DLL 유지,
  게임 코드 다시 빌드 + 불러오기, 새 프로젝트 + 업그레이드(변경 0), 출시 빌드. 전체 약 2분. `-Quick`(빌드 + 단위 테스트 + mp_test, 약 40초).
- **왜**: 범용 에디터 작업 때 Editor 빌드, Release 빌드, 결정론 테스트, 에디터 플레이 캡처, 핫 리로드, 빌드 실패 처리, 출시 빌드를 매번 손으로 돌렸다. 엔진을 고칠 때마다 이걸 다 기억해서 돌리기 어렵다.
- **방식**: 위 과정을 차례로 실행하고 항목별 PASS/FAIL과 걸린 시간을 출력한다. 스크린샷은 `build\check\`에 남긴다. `-Quick`(빌드 + 결정론 테스트만) 옵션.
- **이후**: 헤드리스 시뮬레이션(4단계)이 생기면 GPU 없이 도는 부분을 늘린다.

### [x] 프로젝트 업그레이드 도구 + 엔진 버전 (완료 2026-10-15)
- **구현됨**: 엔진 버전은 저장소 루트 `VERSION`(0.4.0) → CMake가 `MEALKIT_VERSION`으로 넘기고 `mealkit/core/Version.h`(`kEngineVersion`). 에디터 창 제목·프로젝트 관리자에 보인다.
  `mealkit.project`에 `engineVersion=`. 새 프로젝트는 템플릿 파일의 SHA-256을 `.mealkit-template`에 남긴다(`tools/project_common.ps1`을 new_project와 같이 쓴다).
  `tools/upgrade_project.ps1 -Project <폴더> [-DryRun] [-Force] [-Source]`: 해시가 기록과 같은(고치지 않은) 템플릿 파일만 새 템플릿으로 바꾸고,
  고친 파일은 `<파일>.new`로 새 내용을 남긴다(템플릿이 그대로면 조용히 둔다). `mealkit.project`는 engine=·engineVersion=만 고친다. 바꾸기 전 파일은 `.upgrade_backup\<시각>\`.
  `-Source`는 `source/`를 엔진 예제 코드로 바꾸고(예제에 없는 파일은 지움, 백업) 없는 에셋만 더한다.
  에디터는 프로젝트를 열 때 버전이 다르면 업그레이드를 묻고, 누르면 스크립트를 빌드 로그 창에서 돌린 뒤 게임 코드를 다시 빌드한다(자동 테스트 `--upgrade`).
- **Sandbox 이전**: `D:\GameEngine\MealKit\Sandbox`를 `-Source -Force`로 옮겼다(예전 템플릿 그대로임을 git 기록과 비교해 확인, 백업 `.upgrade_backup\20261015-184512`).
  Editor·Release 빌드, `mp_test` 1+3 PASS, 에디터 플레이 확인.
- **왜**: 템플릿(`build.bat`, `.vscode`, `CMakeLists.txt` 형식)이 바뀔 때마다 기존 프로젝트를 손으로 옮겨야 한다(범용 에디터 작업 때 Sandbox를 손으로 옮겼다).
- **방식**: `tools/upgrade_project.ps1 <프로젝트>`가 템플릿이 관리하는 파일만 새로 쓰고 `source/`, `assets/`는 건드리지 않는다. 바꾸기 전 파일은 `.upgrade_backup/`에 남긴다.
  사용자가 고친 템플릿 파일(예: `CMakeLists.txt`의 SOURCES)은 덮어쓰지 않고 차이를 보여 준다.
- **엔진 버전**: 엔진에 `MEALKIT_VERSION`, `mealkit.project`에 `engineVersion=`. 에디터가 프로젝트를 열 때 버전이 다르면 업그레이드를 권한다.

### [x] 코드 서식 통일 (`.clang-format`, `.editorconfig`) (완료 2026-10-15)
- **구현됨**: 저장소 루트와 프로젝트 템플릿에 `.clang-format`(LLVM 기반, 4칸, 120열, 중괄호 같은 줄, 짧은 함수·if·case 한 줄, include 순서는 그대로)과 `.editorconfig`.
  `third_party/.clang-format`은 서식을 끈다. VS Code는 C/C++ 파일을 저장할 때 서식을 맞춘다(`.vscode/settings.json`, 템플릿도).
  기존 엔진·예제 코드에 한 번 적용했다(서식만 바뀐 커밋). 줄 맞춤이 뜻이 있는 행렬 코드는 `// clang-format off`로 둔다.
- **왜**: 프로젝트가 여러 개가 되거나 AI로 코드를 짜도 스타일이 일관되게. 지금 스타일(4칸, 120열, 중괄호 같은 줄)을 규칙 파일로 고정한다.
- **방식**: 엔진과 프로젝트 템플릿에 넣고, VS Code에서 저장할 때 자동 서식. `third_party/`는 제외. 비용 거의 없음.
