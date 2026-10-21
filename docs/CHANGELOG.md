# MealKit 변경 기록

끝낸 로드맵 항목의 구현 상세(무엇을 만들었고, 왜, 어떻게, 남은 것). 앞으로의 계획과 순서는 `docs/ROADMAP.md`, 설계 답은 `docs/CODING_MODEL.md`. 결정의 이유와 넣지 않거나 미룬 기능은 `docs/DECISIONS.md`.
로드맵 항목을 끝내면 상세를 여기로 옮기고 ROADMAP에는 한 줄만 남긴다. 최신 버전이 위에 온다.

## v0.5.0 이후 — 2단계 진행 중 (엔진 버전 그대로, 템플릿 변경 없음)

### [x] 벤치마크 모드 + GPU 예산 나누기 + 메모리 숫자 (완료 2026-10-21)
- **왜**: 그래픽 기능을 넣을 때마다 같은 조건으로 비교할 기준이 필요하고, 성능 예산의 "현재" 값을 최소 사양 실기에서 다시 재야 한다. 공유 메모리 내장 그래픽이라 메모리도 프레임 시간만큼 본다.
- **`--benchmark [초]`** (`game/Benchmark.h`, `GameMain.cpp`): 혼자, 고정 시드(주지 않으면 1), 봇 입력, 수직동기 끔(`--vsync`를 주면 그것), 소리 끔, 자동 실행(플레이어 설정 안 읽음). 매치가 시작된 뒤 3초 데우기를 버리고 기본 30초 재서 `benchmark.txt`(`--benchmark-out`)를 쓰고 끝난다. 콘솔에 `BENCHMARK frames= avg= p99= worst= sim= gpu=` 한 줄. 보고서: 게임 버전·커밋·시뮬레이션 버전·결정론 그룹, CPU, GPU·드라이버·UMA, 화면, 씬·시드, 평균·1% 최악(99번째 백분위, 실행 전체 프레임)·최악 프레임, 틱당 시뮬레이션, 드로우 콜·인스턴스 평균, 프로세스·GPU 메모리 최대와 GPU 예산, 패스별 GPU 시간과 예산(넘으면 `OVER`). `FrameStats` 묶음을 주기 길이로 가중해 쌓는다(`takeSimMsPerTick`을 읽는 곳은 계속 하나).
- **"고정 카메라 경로"**: 카메라는 게임 `view/`가 갖고 있어 엔진이 따로 경로를 두지 않았다. 고정 시드 + 봇이면 같은 빌드·데이터에서 봇 캐릭터를 따라가는 카메라가 같은 길을 간다(프레임 시간에 따른 보간 차이는 남는다). 게임이 정하는 카메라 경로가 필요해지면 4단계 가상 카메라에서.
- **RHI GPU 시간** (`rhi/Rhi.h`): `CommandList::beginTimer`/`endTimer`(구간 8개) + 명령 목록 전체, `Device::gpuTimes()`(2프레임 전, 기다리지 않음). DX12는 타임스탬프 쿼리 힙 + 계속 매핑한 readback 버퍼, 쓴 쿼리만 resolve. Vulkan은 쿼리 풀(프레임 처음에 `vkCmdResetQueryPool`), `timestampValidBits`·`timestampPeriod`, 쓴 쿼리만 읽는다. 타임스탬프가 없으면 `valid = false`.
- **GPU 메모리** (`Device::gpuMemory()`): DX12 `IDXGIAdapter3::QueryVideoMemoryInfo`(로컬 세그먼트), Vulkan은 `VK_EXT_memory_budget`이 있으면 켜고 VMA `vmaGetHeapBudgets`를 장치 로컬 힙만 더한다(없으면 VMA 어림). **프로세스 메모리**는 플랫폼 계층 `processMemory()`(Windows 작업 집합 `K32GetProcessMemoryInfo`, Linux `/proc/self/statm` + `getrusage`, macOS `task_info`).
- **렌더러 패스** (`render::GpuPass`: opaque, shadow, post, vfx, ui): 3D는 `beginFrame` → `beginOverlay`, UI(게임 UI + 디버그 UI)는 그 뒤. 그림자·포스트·VFX는 그 기능이 들어올 때 재기 시작한다(지금 0). `RenderStats`에 `gpuValid`·`gpuFrameMs`·`gpuPassMs`, `Renderer::gpuMemory()`.
- **GPU 예산**: `mealkit.project` `gpuBudgetOpaqueMs` 3 / `gpuBudgetShadowMs` 1.5 / `gpuBudgetPostMs` 1 / `gpuBudgetVfxMs` 1 / `gpuBudgetUiMs` 0.5 / `gpuBudgetSpareMs` 1(패스 밖 `other`와 비교) — `MEALKIT_VIEW_FIELD`라 콘텐츠 해시 밖, [프로젝트 설정] 창에 나온다. 템플릿 파일은 바꾸지 않았다(없는 줄은 기본값).
- **FrameStats**: `FrameSample`에 `p99FrameMs`, `hasGpuTime`·`gpuMs`·`gpuPassMs`, `gpuMemoryUsed`·`gpuMemoryBudget`, `processMemory`·`processMemoryPeak`(렌더러가 출처일 때). 창 제목·F1·`perf:` 로그에 `gpu ... ms`, `mem ... MB`, `vram .../... MB`. 단위 테스트 `frame_stats`에 백분위.
- **검증**: Core Ultra 5 125U(Xe-LPG)에서 DX12 8초: 평균 0.35 ms, 1% 최악 1.57 ms, GPU 0.24 ms(opaque 0.20), 시뮬레이션 1.13 ms/틱, 프로세스 230 MB, GPU 59 MB / 8,318 MB. Vulkan 5초: GPU 0.51 ms(opaque 0.46), GPU 97 MB. `--debug`로 D3D12 디버그 레이어·Vulkan 검증 레이어 메시지 없음. check.ps1.
- **남은 것**: 최소 사양(11세대 Xe-LP) 실기에서 재고 ROADMAP 성능 예산 표와 GPU 예산 기본값을 고친다. 에디터 성능 패널(4단계)에서 같은 표.

### [x] 게임 화면 UI 2부 — Penpot 플러그인, 가져오기 서버, Grid, 배치·픽셀 비교 (완료 2026-10-20)
- **왜**: 프로그래머가 아닌 사람이 Penpot에서 화면을 그리고 그대로 게임에 넣는다(`docs/DECISIONS.md` 2026-10-15). 1부(`.ui` 런타임 + [UI] 미리보기)는 v0.4.16.
- **런타임 Grid** (`ui/Ui.cpp`, `Document`): `layout grid [row|column]`, `columns`·`rows`(고정·`fr`·`auto`·`%`), `cell <행> <열> [합치기]`, 자동 배치, `justifyItems`·`justifySelf`, `justify stretch`(auto 트랙이 남는 자리). 열 먼저 정하고 그 너비로 행 높이(글자 줄바꿈). 노드 `id`(Penpot 도형 id, 배치 비교가 찾는다), `NodeBox::id`·`text`. 글자 셰이핑은 1부 뒤 `ui/Text.cpp`(코드 정리 6).
- **플러그인** (`tools/penpot-plugin`, TypeScript, esbuild로 `dist/`): `snapshot.ts`(Penpot API → JSON, 'mixed' 풀기, 글자 스타일 구간을 반씩 나눠 찾기, variants의 `State` 속성), `convert.ts`(→ `.ui`, Penpot API를 모르는 순수 함수), `ui.ts`(패널: 레이어 바인딩·위젯을 플러그인 데이터에, 화면 내보내기, 에디터 또는 zip), `example.ts`(예제 화면 만들기). 엔진이 못 그리는 것(패스, 불리언, 마스크, 이미지 채우기, 여러 채우기, 3색 이상 그라디언트, 점선, 안쪽 그림자, 블렌드)은 두 배 PNG로 굽고(배경만이면 `skipChildren`), 블러·capitalize·양쪽 정렬은 경고. 효과 없는 그룹은 펼친다. 글자 레이어 이름이 키 모양이면 키(없는 키는 기본 언어 표에 더하고, 있는 키는 표가 이긴다). 글꼴은 `<family>-<weight>` 파일 이름(없으면 오류). 토큰(켜진 세트의 색·숫자) → `theme.ui`(이름별로 합친다). 시험: `node --test test/convert.test.ts`(고정 데이터 → 기대 `.ui`), 엔진 단위 테스트 `penpot_expected`가 기대 파일을 실제 파서로 다시 읽는다.
- **가져오기 서버** (`editor/ImportServer`, 새 `net::TcpListener`·`TcpStream` win32/posix): 프로젝트를 열면 127.0.0.1:27220에 연다. `GET manifest.json/plugin.js/ui.html/icon.png`(주소 자리 `__MEALKIT_BASE__`를 자기 주소로), `GET api/project`(글꼴, 뷰 모델 필드, `.ui`에 쓰인 액션), `POST api/export`(항목 목록 본문: `ui`, `png`, `clean`, `theme`, `strings`, `penpot`, `boxes`, `preview`). [UI] 패널에 주소와 [복사]. 자동 시험은 `--penpot-token`.
- **실제 Penpot(design.penpot.app 2.18.4-RC4)에서 찾아 고친 것**: 플러그인 샌드박스에 `TextEncoder`가 없다(패널만 쓴다). Flex·Grid는 레이어 목록의 위 레이어부터 배치한다(레이아웃 부모는 자식을 거꾸로 쓴다). 레이아웃 안 글자의 `layoutChild`는 `fix`로 와서 grow type을 따른다. 글자 정렬은 문단 범위에서 읽는다. 아이콘 주소는 플러그인 출처 기준. `File.export('penpot')`는 Penpot 쪽 오류라 경고로. 내장 브라우저는 localhost를 막아 Claude in Chrome(Brave)로 시험했다. 글꼴은 Penpot 팀 글꼴에 Pretendard(woff2)를 올려야 했다.
- **검증**: `MealKitEditor --ui-check [--lang]`(배치 상자, 예제 화면 14개 노드: 틀 0.5픽셀 안, 글자 최대 2.4픽셀), `--ui-render`(반 크기 그림) + `tools\compare_images.ps1`(차이 48 넘는 픽셀 0.22%). 기준 샘플 `assets/ui/penpot_example.ui`(Penpot에서 내보낸 일시정지 메뉴, `art/ui/boxes`·`art/ui/preview`)로 check.ps1 세 단계(변환기 시험, 배치, 픽셀).
- **남은 것** (ROADMAP "게임 화면 UI 3부"): 기본 화면 템플릿의 Penpot 원본, 컴포넌트를 재사용 `.ui`(`use`)로, 상태 variants의 실제 Penpot 확인, 토큰 세트별 테마 변형.

### [x] 코드 정리 3·6·7 — 참가 창을 `runGame`으로, 글자 셰이핑 분리, 참가 창 명령줄 공유 (완료 2026-10-20)
- **3. 참가 창을 `runGame`으로**: `game::runGame`을 명령줄 읽기(`runGame(argc, argv, spec)`)와 실행(`runGame(MatchOptions, WindowOptions, spec, GameRunOptions)`)으로 나눴다. 에디터 여러 명 플레이의 참가 창(`editor::runPlayer`)은 자기 루프(창·렌더러·접속·`Frontend`·프레임) 대신 참가 모드 출시 exe와 같은 함수를 부른다. 다른 점은 `game::GameRunOptions`로 받는다: 이미 읽은 프로젝트 설정, 창 제목·제목 표시줄 색, 화면용 튜닝 사본 경로, `playerFiles = false`(설정·바꾼 키·세이브를 읽고 쓰지 않음), `onlineService = false`, `exitWhenSessionEnds`(세션이 끝나거나 게임이 메뉴로 나가면 종료). 소리 끔은 `WindowOptions::mute`. 참가 창도 이제 F1 디버그 UI·V·F12·창 제목의 성능 숫자를 갖는다.
- **6. `ui/Text.cpp`**: `ui/Ui.cpp`(1746 → 1517줄)에서 UTF-8 해석·스타일 구간 태그·커닝·줄 나누기·말줄임을 `ui/Text.h`·`Text.cpp`(`decodeUtf8`, `shapeGlyphs`, `breakLines`)로 옮겼다. 기능 변경 없음.
- **7. 참가 창 명령줄**: 참가 창은 `--player-window` + 에디터 항목 셋(`--project`, `--player-index`, `--view-tuning`)만 직접 읽고 나머지는 출시 exe 파서 `game::parseGameArgs`로 읽는다(`--join`, `--scene`, `--tuning`, `--bot`, `--debug` 등). 에디터 `Options`에서 참가 창 전용 항목(`--player`, `--tuning`, `--view-tuning`, `--player-index`)을 지웠다. 에디터 본 창의 `--players`(플레이 인원)·`--bot`(에디터 창 봇)·`--scene`(편집할 씬)은 게임 옵션과 뜻이 달라 그대로 둔다.
- **검증**: Editor 빌드 경고 없음, 에디터 `--play --players 3 --bot` 두 참가 창 접속·동기화, check.ps1.

### [x] RHI 층 — DX12 + Vulkan 백엔드 (완료 2026-10-20)
- **왜**: 2단계는 bindless 텍스처·그림자·포스트·GPU 타임스탬프로 그래픽 API 호출이 가장 많이 늘어나는 단계라, 그 전에 호출을 한 층으로 모았다. 구현이 하나뿐인 추상화는 DX12의 가정이 새어 나오므로 Vulkan 백엔드를 같은 항목에서 만들었다(`docs/DECISIONS.md` 2026-10-16). Vulkan 하나가 5단계의 Linux·Steam Deck·Android를 덮는다.
- **RHI** (`mealkit/rhi/Rhi.h`): `Device`(장치·스왑체인, 정적 버퍼, 텍스처, 파이프라인, 프레임 업로드 링 `allocUpload`, 캡처, `gpu()`), `CommandList`(`beginScene`/`beginOverlay`, 뷰포트, 파이프라인, 푸시 상수, 정점·인덱스 버퍼, 그리기, 텍스처 배리어·복사), 디버그 UI 그리기(`imguiInit`/`imguiRender`). 모양은 DX12를 따르고 백엔드는 빌드 때 하나(`MEALKIT_RHI`, 가상 함수 없이 `rhi/dx12/`·`rhi/vulkan/`의 `.cpp`). 렌더 그래프는 없다. 백버퍼 배치 전환은 `beginScene`/`beginOverlay`/`endFrame` 안에서, 텍스처 배리어는 부르는 쪽이 명시한다. 복사·배리어는 묶인 그리기 대상을 푼다(Vulkan dynamic rendering 규칙).
- **바인딩** (`shaders/Bindless.hlsli`, `docs/DECISIONS.md` 2026-10-19): 모든 파이프라인이 푸시 상수 64바이트 + `ByteAddressBuffer g_buffers[]` + `Texture2D g_textures[]` + 고정 샘플러 하나. 프레임 데이터(상수·인스턴스·UI 사각형)는 업로드 링에 쓰고 셰이더가 링 번호·오프셋으로 필드마다 `Load`한다(C++ 배치만 맞추면 된다). 루트 CBV·SRV와 `ResourceDescriptorHeap[]` 계획은 버렸다. 셰이더 모델 6.6.
- **렌더러** (`render/Renderer.cpp`): 그래픽 API를 부르지 않는다. `NativeHandles`·`native()` 대신 `device()`. 캡처는 RHI가 RGBA를 돌려주고 플랫폼 계층 `savePng`로 쓴다. GPU 조사(`GpuInfo`, `queryGpu`, `gpuMinimumError`)는 `rhi`로 옮기고 `render`에 별칭을 둔다. `GpuInfo`에 `api`, `unsupported`(백엔드가 확인한 최소 사양 부족)를 더했고 `--system-info`가 API를 출력한다.
- **DX12 백엔드** (`rhi/dx12/`): 예전 렌더러 코드. 셰이더가 보는 서술자 힙 하나(버퍼 16칸 + 텍스처 4,096칸, 디버그 UI 텍스처도 여기서 빌림), 공유 루트 시그니처 1.1(루트 상수 + 무한 범위 표 두 개 + 고정 샘플러). 어댑터 이름의 UTF-16 → UTF-8은 직접 변환(OS 문자열 함수를 쓰지 않는다).
- **Vulkan 백엔드** (`rhi/vulkan/`): Vulkan 1.3 + descriptor indexing(partially bound, update after bind) + dynamic rendering + synchronization2 + `VK_KHR_swapchain_mutable_format`(스왑체인 이미지 하나에 sRGB 뷰 = 장면, UNORM 뷰 = 오버레이). volk가 `vulkan-1.dll`을 실행 중에 불러오고(SDK 없이 빌드), VMA가 메모리를 맡는다. 뷰포트 높이를 음수로 뒤집어 DX12와 같은 셰이더·같은 시계 방향 앞면으로 같은 그림. 수직동기 끔은 `IMMEDIATE`(없으면 `MAILBOX`), 프레젠트 방식이 바뀌면 스왑체인을 다시 만든다. `--debug`면 SDK가 있을 때 `VK_LAYER_KHRONOS_validation`을 켜고 경고·오류를 `[Vulkan]` 로그로. 장치는 최소 사양을 넘는 것 중 외장 > 내장 순서.
- **디버그 UI**: ImGui Win32 쪽은 플랫폼 계층(`platform/ImGuiPlatform.h`), 그리기 쪽은 백엔드(`Dx12ImGui.cpp`, `VkImGui.cpp` + `imgui_impl_vulkan`)로 나눠 `debug/ImGuiLayer.cpp`가 OS·그래픽 API를 부르지 않는다. `tools\check_platform.ps1`의 예외 두 개(`Renderer.cpp`, `ImGuiLayer.cpp`)를 지우고, 그래픽 API 헤더는 `rhi/<api>/`에서만 허용한다.
- **빌드**: `build.bat Vulkan`(`build\Vulkan`, Release 게임)과 `VulkanEditor`(`build\VulkanEditor`, 에디터 + 게임 DLL. 에디터는 자기 빌드 폴더 이름 `editorBuildName()`으로 게임 DLL·플레이 파일·[빌드] 명령을 고른다, 프로젝트의 VulkanEditor는 엔진 `build\VulkanEditor`를 쓴다). 셰이더는 고정 버전 DXC(v1.9.2609, `cmake/Dxc.cmake`가 SHA-256을 확인하고 `%LOCALAPPDATA%\MealKit\tools`에 한 번 받는다, `-DMEALKIT_SHADER_DXC`로 바꿀 수 있다)로 모든 빌드가 DXIL과 SPIR-V를 만든다. 다른 백엔드 것은 `shaders/other/`(검사용, exe에 없음). Windows SDK dxc는 SPIR-V를 못 만들어 쓰지 않는다.
- **외부 라이브러리**: Vulkan-Headers·volk `vulkan-sdk-1.4.357.0`, Vulkan Memory Allocator v3.4.0, `imgui_impl_vulkan`(ImGui와 같은 커밋). 모두 MIT(Vulkan-Headers는 Apache-2.0 OR MIT 중 MIT).
- **검증**: `check.ps1`에 세 단계 — `VulkanEditor` 빌드, DX12 = Vulkan 에디터 편집 화면 캡처 비교(`tools\compare_images.ps1`, 채널 차이 2 초과 픽셀 0.1 % 이하, 상태 표시줄 제외 — 실제로는 최대 차이 1/255, 0픽셀), 검증 레이어를 켠 플레이(메시지 0개, SDK가 없으면 `SKIP`). 동기화 검증(`VK_KHRONOS_VALIDATION_VALIDATE_SYNC`)으로 에디터 플레이·게임 매치·메뉴 흐름 메시지 0개. DX12 쪽은 옮기기 전 HEAD 빌드와 에디터 캡처 3D 영역이 1픽셀 1/255(SM 6.0 → 6.6) 차이. Vulkan exe `--shadow --rewind-test` 통과. check.ps1 26개 통과.
- **남은 것**: Best Practices 검사가 알리는 두 가지 — 배리어 접근 마스크를 넓게 잡은 것(`imageBarrier`, 프레임에 몇 번뿐이라 일부러), 푸시 상수 64바이트 중 일부만 채우는 것. Vulkan의 Tracy GPU 시간 측정. 푸시 뒤 Windows CI에서 DXC 받기 확인. 타임스탬프 쿼리는 벤치마크 모드(GPU 예산)에서 더한다.
- **다른 백엔드 (5단계)**: Metal은 argument buffer tier 2, WebGPU는 bindless가 없어 머티리얼 묶음별 드로우(웹 프리셋, 드로우 콜 예산 따로). 셰이더는 SPIR-V에서 변환(후보 SPIRV-Cross, Tint/Naga), 변환은 빌드 때만.

### [x] 코드 정리 4·5 — 에디터 패널 나누기, 코어 include 검사 (완료 2026-10-19)
- **4. `editor/Editor.cpp` 나누기** (1806 → 1500줄): 튜닝 문서·프로젝트 설정 창·프로젝트 빌드를 [표]·[연출]처럼 패널 클래스로 뺐다. `TuningPanel`(game.tuning·view.tuning 문서, 플레이 사본, 행 참조 풀기), `ProjectSettingsPanel`(고치는 중인 값만 갖고 저장할 때 에디터의 `ProjectInfo`를 바꾼다), `BuildPanel`(`build.bat`·`upgrade_project.ps1` 실행, 출력 모으기, [빌드 로그] 창, 상태 표시줄 "빌드 중"). 빌드 전 저장(출시본은 씬·프로젝트 설정)과 끝난 뒤 할 일(DLL 다시 읽기, 업그레이드 뒤 다시 빌드)은 에디터에 남았다. 패널 상태 문구는 `takeStatus()`로 에디터 상태 표시줄에 간다. Ctrl+S와 [파일] > [저장]이 같은 `Editor::saveAll()`을 쓴다. `BuildKind`는 `BuildPanel.h`로 옮겼다(`editor::BuildKind` 이름 그대로). 동작 변경 없음.
- **5. 코어 → 화면 쪽 include 검사** (`tools\check_layers.ps1`): `engine/CMakeLists.txt`의 `MEALKIT_CORE_SOURCES` 파일이 같은 목록의 헤더만 include하는지 본다. 폴더 순서 검사로는 같은 `game/` 안의 코어 → `Frontend.h`·`GameMain.h`·`FrameStats.h` include를 못 잡아 Linux·macOS CI 링크에서야 드러났다. 이때 헤더만 있는 `game/BuildInfo.h`·`platform/Input.h`·`platform/Gamepad.h`(코어 입력 액션이 쓴다)를 코어 목록으로 옮겼다(빌드 결과는 같다). `docs/CODING_MODEL.md` 검사 7.
- **검증**: check.ps1 23개 통과, 일부러 코어 파일에 `Frontend.h`를 넣으면 검사가 실패하는 것 확인, 에디터 캡처(프로젝트 설정 창·튜닝 패널).

## v0.5.0 — 1단계 완료 (2026-10-19)

1단계("게임이 되게 하는 최소 기능")의 마지막 항목을 끝내 v0.5로 올린다(ROADMAP "버전": v0.n = n단계를 끝낸 엔진).

### [x] 기준 샘플 (완료 2026-10-19)
- **v0.5 조건**: sandbox(물리 파티 대전)가 타이틀 → 메뉴 → 로비(LAN 방·주소·Steam, 이름·몸·준비·채팅) → 로딩 → 매치(카운트다운·2분·HUD·일시정지) → 결과 → 로비를 끝까지 돈다. 1단계 기능(소리, UI, 입력·게임패드, 설정, 세이브, 로드아웃, 화면 흐름, 네트워크 표시, 접근성, 버전)이 모두 이 샘플에서 쓰인다.
- **검증**: check.ps1 "Game screen flow (title to match)"(`--ui-press`로 타이틀 → 혼자 하기 → 시작), 두 프로세스 LAN 참가·준비·시작, 결과 → 로비 → 두 번째 매치, `mp_test` 1+3(창·헤드리스, 네트워크 흉내 포함), clang-cl 클라이언트 비교, 60 Hz.
- **v0.5를 막지 않아 넘긴 것**: 게임 화면 UI 2부(Penpot 플러그인, 디자이너가 생기면), 코드 정리 3~7, 게임 코드용 시나리오 테스트. ROADMAP 1단계 절에 남아 있다.
- **영향**: 엔진 버전 0.5.0. 기존 프로젝트는 `tools\upgrade_project.ps1`로 템플릿을 올리고, 화면 흐름을 쓰려면 `-Source`(게임 코드를 예제로, 백업) 또는 sandbox의 `app/Flow.*`·`Screens.h`·화면 `.ui`를 가져간다. 문서 저장소 `ENGINE_VERSION` 0.5(0.4 문서는 `0.4` 브랜치).

## v0.4.31 — 1단계 진행 중 (2026-10-19)

### [x] 버전 표시와 접속 거부 이유 (완료 2026-10-19)
- **왜**: 실행 파일·데이터가 다르면 접속이 거부됐지만 이유를 알 수 없었고, 실행 파일 id(컴파일 시각) 비교는 화면 코드만 다른 빌드끼리도 막았다.
- **빌드 정보** (`cmake/BuildInfo.cmake` → `<게임>_build_info/mealkit_build_info.h`, 빌드마다, 바뀔 때만 다시 쓴다): 게임 버전(`mealkit.project` `version`, 기본 0.1.0), 프로젝트 폴더의 git 커밋(고친 파일이 있으면 `+`), **시뮬레이션 버전** = 게임 버전 + 시뮬레이션에 닿는 소스(게임 `sim/`·헤드리스 파일·엔진 `core`·`net`·`physics`·`sim`·`data`·`scene`·`asset`·`game`의 시뮬레이션 파일·Jolt `mealkit.txt`)의 SHA-1(줄바꿈 LF, 상대 경로). 같은 소스면 OS·컴파일러가 달라도 같다. `game::BuildInfo`와 `MEALKIT_THIS_BUILD`(게임 타깃에서만 헤더를 본다) → `GameMainSpec::build`, `GameModule::build`(`kApiVersion` 21), 헤드리스.
- **보이기**: 창 제목 `게임 v0.1.0 | …`, 시작 로그 `Build: version …, commit …, simulation …, msvc-x64`, sandbox 타이틀(`v0.1.0 (커밋) · MealKit 0.4.31`)과 로비 구석.
- **접속 비교** (`net/Lockstep.*`): 실행 파일 id 대신 결정론 그룹(`determinismGroup()`: `msvc-x64`·`clang-x64`·`clang-arm64` …, 실기기 결정론 확인 전까지 다르면 거부) → 시뮬레이션 버전(`setBuild`, 없으면 `buildId`) → 콘텐츠 해시(`setContentParts`로 씬·모델·튜닝·프로젝트·표 다섯 구성도 보낸다). 거부 패킷에 이유·다른 구성·방장의 게임 버전·그룹을 실어 클라이언트가 `rejectInfo()`로 안다. `--any-build`는 앞의 둘을 건너뛴다. `game::MatchData::contentParts`.
- **LAN 방**: 알림에 시뮬레이션 버전·게임 버전·그룹(형식 버전 2), 참가 목록이 "버전이 다름 (1.3)"·"데이터가 다름"으로 표시.
- **sandbox**: 알림 화면이 거부 이유를 문자열 표로("게임 버전이 방장과 다릅니다 (내 1.2 / 방장 1.3)", "게임 데이터가 방장과 다릅니다: 튜닝", 제목 "방에 들어갈 수 없습니다").
- **출시 폴더**: `mealkit.project` `exportVersionFolder`(기본 끔)가 켜지면 `export\<이름>-<version>\`.
- **검증**: 단위 테스트 `join_reject_reasons`(시뮬레이션 버전이 다르면 Version + 방장 버전, 튜닝·표가 다르면 Content + 그 구성). 두 창에서 다른 튜닝으로 참가 → "게임 데이터가 방장과 다릅니다: 튜닝" 캡처. check.ps1.
- **영향**: 기존 프로젝트도 빌드 정보 헤더가 생긴다(`mealkit_add_game`). 엔진 코어 폴더를 고치면 시뮬레이션 버전이 바뀐다(보수적). `docs/DECISIONS.md` 2026-10-19.

## v0.4.30 — 1단계 진행 중 (2026-10-19)

### [x] 틱레이트를 프로젝트 설정으로 (완료 2026-10-19)
- **왜**: 30 Hz 고정이었다. 물체가 적은 액션 게임은 60 Hz가 반응·충돌에 낫다.
- **설정**: `mealkit.project` `tickRate`(30 기본, 60 선택, 10~120). 콘텐츠 해시에 들어간다(모두 같아야 한다). `game::applyTickRate`가 프로세스 틱레이트(`reflect::setTickRate`)로 정한다: `loadMatchData`(출시 exe·헤드리스·에디터 플레이·참가 창)가 시간 필드(튜닝·씬·표의 초)를 읽기 전에, 에디터는 프로젝트를 열 때와 프로젝트 설정을 저장할 때(다음 플레이부터).
- **엔진**: `game::tickDt()`·`msToTicks()`(`mealkit/game/ProjectSettings.h`). 헤드리스 루프가 `Game::kTickDt` 대신 `tickDt()`를 쓰므로 헤드리스 클래스의 `kTickDt`는 더 필요 없다. `--delay`는 이제 **밀리초**(기본 100, 틱은 프로젝트를 읽은 뒤 계산), 에디터 플레이도 100 ms. 입력 지연 자동 조절은 이미 틱 길이로 계산한다.
- **sandbox** (템플릿): `sim/World.h`의 `kTickRate`·`kTickDt` 상수를 없애고 `World::tickRate`·`tickDt`(매치 시작 때 `reset`이 받는다, `Headless::startWorld`가 `reflect::tickRate()`를 넘긴다). 이동 가속·물리 단계·카운트다운 초가 이것을 쓴다. 30 Hz 결과는 그대로(고정 기록 해시 그대로).
- **영향**: 기존 프로젝트는 그대로 빌드된다(헤드리스 클래스의 `kTickDt`는 쓰이지 않는다). `tickRate`를 바꾸려면 게임 `sim/`의 틱 상수를 sandbox처럼 `World`로 옮긴다.
- **검증**: `tickRate 60`으로 헤드리스 1+3 1800틱 PASS, 그림자 시뮬레이션 일치, HUD 남은 시간이 초 단위로 맞다(690틱 = 11.5초에서 1:51). 30 Hz check.ps1.

## v0.4.29 — 1단계 진행 중 (2026-10-19)

### [x] 접근성 기본 묶음 (완료 2026-10-19)
- **흔들림·번쩍임·화면 정지 줄이기**: 플레이어 설정 `gameplay.shakeScale`·`flashScale`(0~100 %, 설정 화면 [게임] 탭 슬라이더)·`hitStop`(토글). 설정 서비스가 `feedback::Player::setAccessibility`로 넘기고, 연출 표는 그대로 두고 결과(카메라 흔들림 값, 화면 색 양)에 곱한다. 화면 정지를 끄면 `hitStop()`이 늘 false. `settings::Service::Targets::feedback`.
- **자막**: 소리 이벤트 표(`audio::SoundEvent`)에 `subtitle`(게임 문자열 키)·`subtitleSeconds`(2.5) 열. 재생하면 `audio::Engine::subtitles()`에 줄이 쌓이고(같은 키는 한 줄, 시간만 늘린다, 볼륨·음소거와 무관), `game::Frontend`가 플레이어 설정 `gameplay.subtitles`가 켜져 있고 프로젝트에 `assets/ui/subtitles.ui`가 있으면 표준 자막 화면(층 5, 뷰 모델 `SubtitlesVm` 최근 4줄)을 연다.
- **sandbox**: 밀치기·점프대·나타나기·떨어지기 소리에 자막(`sub.*` 문자열), 템플릿에 `assets/ui/subtitles.ui`.
- **남은 것**: 색맹 보정 LUT 세 개는 2단계 포스트 프로세싱 항목에서(로드맵대로).
- **결정론**: 영향 없음(화면 쪽만).
- **검증**: 봇 매치 캡처에서 "[밀치는 소리]" 자막, check.ps1.

## v0.4.28 — 1단계 진행 중 (2026-10-19)

### [x] 내 캐릭터 겉모습만 예측 (완료 2026-10-19)
- **왜**: 입력 지연(60~270 ms) 때문에 내 조작이 늦게 느껴진다. 롤백 넷코드는 보류 표대로 하지 않는다.
- **엔진** (`game::Simulation`): 보냈지만 아직 실행되지 않은 내 입력을 목표 틱 순으로 갖고 `forEachPendingInput(f(틱, 입력))`으로 준다(입력 지연 n틱이면 n개, 지연이 줄어 겹친 틱은 넣지 않는다).
- **sandbox** (`view/Prediction.*`, 템플릿): 아직 실행 안 된 이동 입력마다 한 틱 동안 갈 거리(시뮬레이션 `movePawn`과 같은 속도, 몸 종류 배율)를 더해 내 캐릭터·카메라 초점·이름표를 앞서 그리고, 그 차이로 부드럽게 따라간다(`view.tuning` `prediction`: 켜기, 최대 1.5 m, 따라가기 14/s). 경기 중이고 내 캐릭터가 있을 때만. 확정 상태가 따라오면 차이가 저절로 줄어든다.
- **한계**: 수평 이동만 미리 보인다. 점프·밀치기·다른 캐릭터·상자와의 충돌, 효과(밀치기 고리)는 확정 위치에 보인다.
- **결정론**: 영향 없음(`view/`만, 시뮬레이션 상태를 읽기만 한다).
- **검증**: `--delay 8` 두 창 매치에서 내 캐릭터가 확정 위치보다 앞서 그려지는지 캡처로 확인, check.ps1.

## v0.4.27 — 1단계 진행 중 (2026-10-19)

### [x] 입력 지연 자동 조절 (완료 2026-10-19)
- **왜**: 입력 지연이 `--delay 3`(100 ms) 고정이라 LAN에서는 길고, 먼 상대와는 짧아 매치가 입력을 기다리며 멈칫거렸다(왕복 240 ms에서 초당 12틱).
- **시작 값**: `LockstepSession::setAutoDelay(AutoDelay{enabled, tickMs, minTicks, maxTicks})`. 호스트가 `startMatch` 때 로비에서 잰 가장 큰 왕복 시간으로 `delayForRtt`(왕복 × 1.25 + 한 틱, 틱으로 올림, 최소·최대 사이)를 정해 시작 패킷으로 보낸다.
- **매치 중**: 호스트가 1초마다 다시 보고, 더 필요하면 그 값으로 바로, 2틱 넘게 남으면 1틱 줄이는 **엔진 확정 명령**(타입 `mealkit.net.InputDelay`, 게임 명령과 같은 길이라 입력 기록에도 남는다, 게임의 `forEachCommand`는 모르는 타입이라 건너뛴다)을 보낸다(변경 사이 3초). 모든 피어가 그 명령이 든 틱을 실행한 직후 바꾸므로 같은 틱부터 적용된다. 늘 때 생기는 빈 틱은 각자 마지막 입력으로 채우고, 줄 때 겹치는 틱의 입력은 건너뛴다(호스트는 확정된 틱을 바꾸지 않는다). `requestInputDelay(ticks)`로 직접 바꿀 수도 있다.
- **설정**: `mealkit.project`의 `inputDelayAuto`(기본 켬)·`inputDelayMinMs`(60)·`inputDelayMaxMs`(270), 해시 밖(호스트가 정한다). 출시 exe·헤드리스가 켜고(`autoDelayOf`), `--delay <ticks>`를 주면 고정. 에디터 플레이는 지금처럼 3틱 고정. `MatchService::slowPlayers()`로 로비가 상한보다 먼 사람을 경고한다(sandbox `lobby.slow`).
- **결정론**: 지연은 전달 시점만 바꾼다. 바꾸는 틱은 확정 명령으로 모두 같다.
- **검증**: 단위 테스트 `input_delay_change`(매치 중 3 → 7 → 2틱, 두 피어가 같은 틱에 바꾸고 입력 합이 같다, `delayForRtt` 값). `mp_test` 창 1+2 `-Lag 80 -Jitter 20 -Loss 3`: 2 → 8틱으로 올라 600틱이 약 25초(전에는 48초), 헤드리스 1+3 `-Lag 120 -Jitter 40 -Loss 5` PASS.
- **남은 것**: 플랫폼 종류(PC 유선/콘솔/모바일/웹)별 기본값·상한은 그 플랫폼을 시작할 때(5단계) 실측으로. 상한을 넘는 피어를 게임 설정으로 막는 것은 필요해질 때.

## v0.4.26 — 1단계 진행 중 (2026-10-19)

### [x] 플레이어용 네트워크 표시 (완료 2026-10-19)
- **왜**: 락스텝은 한 명이 느리면 모두 멈추는데, 숫자는 개발용 창 제목·F1에만 있었고 플레이어는 누구 때문에 멈췄는지 몰랐다.
- **세션**: 호스트가 로비·매치 동안 클라이언트마다 왕복 시간을 잰다(로비·묶음 패킷의 호스트 시각을 클라이언트가 접속 요청·입력 패킷에 되돌려 준다, 지수 평균). 모두의 값을 로비 명단(`LobbyMember::pingMs`)과 묶음에 실어 `pingMs(player)`가 모든 피어에서 같다. 확정 틱이 0.15초 넘게 멈추면 그 틱의 입력이 없는 플레이어를 묶음에 실어 `waitingFor()`(비트), 클라이언트는 묶음이 0.3초 넘게 없으면 호스트(비트 0). 매치 중에도 0.5초마다 명단(`Roster` 패킷: 이름·핑, 로비로 보내지 않는다)을 보내, 자동 시작으로 로비 신호를 받기 전에 시작한 클라이언트도 이름을 안다.
- **표시** (sandbox 템플릿): HUD 점수판의 플레이어별 핑(초록, `view.tuning` `badPingMs` 150 이상이면 빨강, 이름은 줄임표), 멈추면 위쪽에 "P3 기다리는 중..."(문자열 `hud.waiting`), 로비 줄의 핑. 플레이어 설정 `gameplay.showNetwork`(설정 화면 [게임] 탭)로 끈다. 혼자 하기에는 보이지 않는다.
- **결정론**: 영향 없음(틱 밖 숫자).
- **검증**: `net_conditions` 테스트에 호스트가 잰 핑, 클라이언트가 입력을 멈추면 양쪽이 플레이어 1을 기다린다는 검사. `--net-lag 400`으로 두 창 매치를 돌려 HUD 확인.

## v0.4.25 — 1단계 진행 중 (2026-10-19)

### [x] 네트워크 상태 흉내 내기 (완료 2026-10-19)
- **왜**: 같은 PC 시험은 지연이 0에 가까워, 입력 지연 자동 조절·끊김 처리를 실제 친구 없이 시험할 수 없었다.
- **방식** (`mealkit/net/Conditions.h`): `withConditions(전송, {지연, 흔들림, 손실})` 래퍼가 보내는 패킷과 받는 패킷을 각각 손실 확률로 버리고, 나머지는 지연의 반 + 0..흔들림만큼 큐에 두었다 내보낸다(순서가 바뀔 수 있다). UDP·Steam 어느 전송에도 씌운다. `LockstepSession::setConditions`(host/join 전, `reset`해도 남는다), 매치 옵션 `--net-lag <ms>`(왕복 시간)·`--net-jitter <ms>`·`--net-loss <%>`(출시 exe·헤드리스, `MatchServiceConfig::conditions`), `mp_test.ps1 -Lag -Jitter -Loss`(모든 피어, 시간 제한도 늘린다).
- **결정론**: 영향 없음(전달 시점만). 난수는 시뮬레이션 밖 `Pcg32`.
- **검증**: 단위 테스트 `net_conditions`(60 ms·흔들림 30·손실 15%에서 90틱, 양쪽 입력 합이 같고 클라이언트 rtt가 지연을 보인다). `mp_test -Headless -Clients 3 -Lag 120 -Jitter 40 -Loss 5` PASS, 창 1+2 `-Lag 80 -Jitter 20 -Loss 3` PASS. 왕복이 입력 지연(100 ms)보다 길면 매치가 입력을 기다리며 느려진다(약 240 ms에서 초당 12틱) — 다음 항목 "입력 지연 자동 조절"이 고친다.

## v0.4.24 — 1단계 진행 중 (2026-10-19)

### [x] 기본 화면 흐름 템플릿 + LAN 방 찾기 + 텍스트 채팅 (완료 2026-10-19)
- **왜**: 호스트·참가를 명령줄로만 고를 수 있었고, 세션은 매치 한 번이면 끝이라 결과 뒤 로비로 돌아갈 수 없었다. v0.5 목표("기준 샘플이 타이틀 → 로비 → 매치 → 결과 → 로비를 끝까지 돈다")의 중심 항목.
- **세션** (`net/Lockstep.*`): 세션 하나로 매치 여러 번 — `returnToLobby()`(호스트, 클라이언트는 로비 신호를 받고 돌아온다, 준비가 풀린다), `matchIndex()`(시작·로비 신호에 실려 지난 매치의 늦은 시작·입력·묶음 패킷을 버린다), `openOffline()`(혼자 하기도 로비부터), `reset()`(Idle로, 이름·콘텐츠 해시는 남는다). 로비 정보: `setLocalName`·`setLocalReady`, 호스트가 모은 `lobbyMembers()`(번호·이름·준비·방장·로드아웃, 로비 신호가 못 담으면 다음 신호가 이어서). 나간 사람의 번호를 다시 쓰고, 시작할 때 빈 번호 없이 다시 매긴다. `net::buildId()`·`clampUtf8` 공개.
- **채팅**: `sendChat(줄)`/`chatLog()`(최근 50줄). 클라이언트 → 호스트(확인받을 때까지 0.1초마다 다시), 호스트가 번호를 붙여 모두에게 같은 순서로 중계(확인받을 때까지 다시). 120바이트, 0.5초에 한 줄. 틱 밖이라 입력 기록·리플레이에 들어가지 않는다.
- **LAN 방 찾기** (`net/Rooms.h`): 호스트가 로비 동안 1초마다 방 알림(게임 이름, 빌드·콘텐츠 해시, 인원, 포트, 방장 이름)을 브로드캐스트와 127.0.0.1로(`--local`이면 127.0.0.1만), 참가 메뉴가 UDP 27199를 여럿이 같이 듣는다(`SO_REUSEADDR`). 같은 게임만, 버전·데이터가 다르면 회색. 플랫폼 계층에 `UdpSocket::openShared`·`enableBroadcast`, `userName()`.
- **엔진 `game::MatchService`** (`mealkit/game/MatchService.h`, `GameContext::match`): 세션을 갖고 메뉴가 부르는 함수 `playOffline`·`host`·`hostOnline`(Steam)·`join`·`joinOnline`·`start`·`returnToLobby`·`leave`·`setBrowsing`/`rooms`, 단계 `phase()`. 출시 exe·에디터 플레이·참가 창이 하나씩 갖는다(명령줄도 같은 함수). Steam 로그인은 빌드에 들어 있으면 시작할 때(메뉴의 Steam 버튼, 친구 초대 수락 → 바로 참가). 기본 이름은 온라인 계정 → 컴퓨터 사용자 이름.
- **`GameInstance`** (`kApiVersion` 20): `menus()`(true면 명령줄 없이 실행할 때 메뉴부터, 엔진이 Esc·Enter를 쓰지 않는다. false인 예전 게임은 동작 그대로), `stop(ctx)`(Running이 끝나면 한 번 — 로비로, 나감, 끊김). 엔진은 Running이 되면 한 프레임(로딩 화면) 뒤 `start`. 매치 전에도 `stateHash`·`activeBodies`·`stateView`가 불린다(0·빈 값). `GameContext::scene`(메뉴 뒤 배경). `Window::close()`.
- **게임 UI**: 글자 입력 위젯 `textInput <필드> [액션]`(글자 노드, `placeholder`, `maxLength`): 키보드 글자(`Input::text`, `WM_CHAR` UTF-16 → UTF-8, 한글 입력기 조합 결과)·지우기, Enter면 액션(기본 `submit`), Esc면 그만, 치는 동안 게임은 키보드를 받지 않는다. `System::focusText`·`editingText`·`pressAny`.
- **시험 옵션**: `--frames <n>`(그리고 끝, `--capture <png>`에 틱이 없으면 마지막 프레임), `--ui-press <프레임:노드,...>`(위젯을 이름으로 누른다). check.ps1 "Game screen flow (title to match)".
- **sandbox** (템플릿): `app/Flow.*`(화면 고르기·버튼), `app/Screens.h`(뷰 모델), 화면 `assets/ui/{title,menu,join,lobby,loading,pause,results,message}.ui`, HUD에 카운트다운·"시작!"·남은 시간·채팅. 시뮬레이션 매치 단계 `MatchPhase`(카운트다운 3초 → 경기 120초 → 끝, 튜닝 `countdown`·`matchLength`, 그동안 입력·AI·점수 없음, 이벤트 Countdown·MatchStart·MatchEnd → 연출 표). 결과는 적게 떨어진 순, 방장은 버튼이나 20초 뒤 모두 로비로(`view.tuning` `resultsSeconds`). 로비: 이름(세이브 `playerName`), 몸 선택(로드아웃), 준비, 채팅, 방장 시작(모두 준비하면). 일시정지(Esc/Start): 혼자 하기는 매치가 멈추고 여러 명이면 메뉴만. 입력 액션 `menu.pause`(Esc, Start)·`chat.open`(Enter), `menu.settings`의 패드 Start를 뺐다.
  상태 필드가 늘어 고정 기록을 다시 만들었다(`arena_4p_seed7.rec`, 최종 해시 `6f09aeef2d0059f3`).
- **검증**: 단위 테스트 `lobby_chat_rematch`(UDP 루프백: 로비 목록·로드아웃, 채팅 순서, 매치 → 로비 → 두 번째 매치, 나간 사람, LAN 방 알림). 두 프로세스로 LAN 방 찾기 → 참가 → 준비 → 시작, 혼자 하기 결과 → 로비 → 두 번째 매치를 `--ui-press`로 확인. check.ps1 전체.
- **남은 것**: Steam 로비 흐름은 컴파일만 확인했다(실접속은 2026-10-18 결정대로 가정). 채팅에 금칙어·신고 없음(로드맵대로). 게임패드로 글자 입력은 없다(화면 키보드는 콘솔 항목에서). 끊긴 이유는 아직 영어 로그 문장이다("버전 표시와 접속 거부 이유" 항목에서 문자열 키로).
- **영향**: 기존 프로젝트는 그대로 빌드·동작한다(`menus()` 기본 false). 흐름을 쓰려면 sandbox의 `app/Flow.*`·`Screens.h`·화면 `.ui`·문자열·바인딩 줄을 가져간다(`tools\upgrade_project.ps1 -Source`).
- **빌드 주의**: 에이전트 셸처럼 콘솔이 UTF-8이면 MSVC의 `/showIncludes` 줄이 ninja의 접두어(CP949)와 달라 헤더 의존이 기록되지 않는다. `cmd /c "chcp 949 >nul & build.bat"`로 빌드한다(그렇지 않으면 헤더만 바뀐 경우 오래된 오브젝트가 남는다).

## v0.4.23 — 1단계 진행 중 (2026-10-19)

### [x] 코드 정리 1·2 — 매치 데이터 한 곳, 매치 옵션 하나 (완료 2026-10-19)
- **왜**: 출시 exe(`GameMain.cpp`), 헤드리스(`loadHeadlessData`), 에디터 플레이·참가 창(`playContentHash`)이 각자 씬·튜닝·게임 프로젝트 설정을 읽고 `contentHash`에 같은 일곱 가지를 넘겼다. 한 곳만 빠뜨려도 피어 해시나 기록 재생이 갈린다. 명령줄도 `parseArgs`와 `parseHeadlessArgs`, `--help` 두 벌이었다. 기본 화면 흐름이 메뉴에서 같은 값을 채우기 전에 하나로 모았다.
- **매치 데이터** (`mealkit/game/GameData.h`): `game::loadMatchData(GameTypes, MatchDataOptions, models, MatchData&)` 하나가 프로젝트 설정(실행 파일 옆에서 찾거나 에디터가 넘긴 것) + 게임 `game.*` 값, 씬(넘긴 씬 / 파일 / 시작 씬, 없으면 기본 아레나), 튜닝, 콘텐츠 해시를 만든다. 게임 타입은 `GameTypes`(오브젝트 종류·튜닝·게임 프로젝트 설정·표): 출시 exe·에디터는 `gameTypes(module)`, 헤드리스는 게임 클래스에서. `contentHash`를 부르는 곳은 이제 이 함수뿐이다. `HeadlessData`·`loadHeadlessData`는 없어졌다.
- **매치 옵션** (`mealkit/game/GameOptions.h`, 엔진 코어): `MatchOptions`(방 만들기·참가·Steam·인원·시드·자동 시험 출력, 예전 `HeadlessOptions`는 같은 타입의 다른 이름으로 남았다)와 `WindowOptions`(창·렌더러·화면 시험), 파서 `parseGameArgs(argc, argv, match, window)` 하나와 `printGameUsage`(매치·시험·창 묶음). 헤드리스는 `window`를 넘기지 않아 창 옵션을 받고 무시하고, Steam·`--replay-inputs`는 한쪽에서만 받는다. 게임 헤드리스 클래스의 `defaults`는 이제 `game::MatchOptions&`를 받는다(sandbox).
- **영향**: `kApiVersion`은 그대로. 기존 프로젝트의 `defaults(game::HeadlessOptions&, …)`는 그대로 빌드된다. 출시 exe에서 잘못된 씬 파일은 창을 만들기 전이 아니라 렌더러를 만든 뒤에 실패한다.
- **검증**: check.ps1 -Quick(결정론 1+3, 헤드리스 기록·재생, 고정 기록 해시 그대로).

## v0.4.22 — 1단계 진행 중 (2026-10-19)

### [x] 프레임 루프 합치기 — `game::Frontend` + 엔진의 출시 exe main (완료 2026-10-19, 로드맵 밖)
- **왜**: 창 있는 게임의 화면 쪽 루프가 세 곳에 복사되어 있었다. 출시 exe(sandbox `app/main.cpp` 558줄, 게임 코드라 프로젝트마다 사본), 에디터 플레이(`PlaySession::update`), 참가 창(`runPlayer`)이다. 서비스 하나(소리·연출·입력 액션·패드·문자열·UI·설정·성능 숫자)를 더할 때마다 세 곳과 모든 프로젝트의 `main.cpp`를 고쳐야 했고, 갱신 순서도 조금씩 달랐다. 다음 항목인 기본 화면 흐름이 네 번째 사본이 될 참이었다.
- **서비스 묶음** (`mealkit/game/Frontend.h`, `docs/CODING_MODEL.md` "창 있는 게임을 돌리는 프레임 루프"): `game::Frontend`가 입력 액션·게임패드·문자열 표·게임 UI·플레이어 설정·화면용 튜닝(`LiveTuning`)을 갖는다. `init(FrontendConfig)`(바인딩·문자열·UI·설정 읽기, 화면 항목을 적용할 창, 설정 파일, 화면용 튜닝 타입), `context()`(`GameContext`), 매 프레임 `update(now, dt, input, padAllowed, ctx)`(문자열 → 화면용 튜닝 → 패드 → UI가 마우스·키보드를 먼저 → 설정 화면 → 액션), `afterBuildFrame(frame)`(월드 위 UI 카메라, 듣는 위치, 연출 화면 색), `renderUi`. 소리·연출은 주인이 만들어 빌려 준다(에디터는 [소리]·[연출] 미리보기와 같이 쓴다).
  출시 exe·에디터 플레이·참가 창이 모두 이것을 쓴다. 에디터 플레이는 렌더러를 받는 첫 `update`에서 `init`하고(플레이마다 다시 읽는다), 패드는 UI가 키보드를 가져가도 넘긴다(전에는 막혀서 게임 UI를 패드로 조작할 수 없었다).
- **엔진의 main** (`mealkit/game/GameMain.h`): `game::runGame(argc, argv, GameMainSpec)`이 sandbox `main.cpp`가 하던 일을 그대로 한다. 명령줄과 `--help`(이름은 exe 이름), `--system-info`, 프로젝트·씬·튜닝·콘텐츠 해시, 세션(혼자·호스트·참가·Steam 로비), `Frontend`, 디버그 UI(F1), 자동 시험 출력(`FINAL`, `SHADOW`, `REWIND`, `perf:`), 창 제목(`game.title`, `title.*` 문자열 키)이다. 게임 타입은 `GameModule`(에디터 DLL과 같은 정보, `gameModuleOf<GameClass>`로 두 매크로가 같이 만든다)로, 기본 포트·인원은 헤드리스 클래스의 `defaults()`로 받는다.
  게임의 `app/main.cpp`는 `MEALKIT_GAME_MAIN(sandbox::GameSession, sandbox::Headless)` 한 줄이다(`--headless`도 이 매크로가 나눈다). 출시 exe도 `GameInstance` 가상 호출을 쓴다(프레임당 몇 번).
- **바뀐 동작**: 디버그 UI 도움말 줄에서 sandbox 전용 `F8: shockwave`가 빠졌다(sandbox 디버그 UI의 버튼에 있다). `--bodies`는 `--match-data`의 다른 이름으로 남았다.
- **영향**: `kApiVersion`은 그대로(`GameModule` 모양은 같다). 기존 프로젝트의 `main.cpp`는 그대로 빌드되지만, 엔진 기능을 따라가려면 sandbox처럼 한 줄로 바꾼다(`tools\upgrade_project.ps1 -Source`는 게임 코드 전체를 예제로 바꾼다). 템플릿 설명(CLAUDE.md, README, CMakeLists 주석)을 고쳐 버전을 올렸다.
- **검증**: check.ps1(출시 exe의 결정론·그림자·되감기·캡처, 에디터 플레이 혼자·3명, 새 프로젝트 + 업그레이드).

## v0.4.21 — 1단계 진행 중 (2026-10-19)

### [x] 성능 숫자 하나로 모으기 — `game::FrameStats` (완료 2026-10-19, 로드맵 밖)
- **왜**: 성능 숫자를 모으고 보여 주는 코드가 세 곳에 따로 있었다. 게임 exe(sandbox `app/main.cpp`: 0.25초 평균, 직접 만든 `snprintf` 한 줄, F1 오버레이는 그 글자를 `" | "`로 잘라 씀), 에디터 상태 표시줄(fps 지수 평균), 에디터 플레이 상태 줄(`takeSimMsPerTick` 직접 호출)이다. 같은 fps가 화면마다 다르게 계산되고, 읽으면 비워지는 `takeSimMsPerTick`을 두 곳이 부르면 값을 나눠 가진다. 벤치마크 보고서(2단계)·성능 패널(4단계)이 네 번째·다섯 번째 사본이 될 참이었다.
- **방식** (`docs/CODING_MODEL.md` "성능 숫자 모으기와 보여 주기"): 측정은 그대로 각 하위 시스템의 `stats()`(`RenderStats`, `net::SessionStats`, `audio::Stats`, `GameInstance::takeSimMsPerTick`). `mealkit/game/FrameStats.h`의 `game::FrameStats`가 모은다. 엔진 루프가 매 프레임 `update(dt, FrameSources{renderer, session, audio, game})`를 부르고, 보고 주기(기본 0.25초)가 끝난 프레임에 `true`를 돌려준다. 그때 `sample()`(`FrameSample`: 평균·**최악** 프레임, fps, 틱·시뮬레이션 시간, 플레이어·rtt·대역폭·디싱크, 드로우 콜·인스턴스·vsync, 목소리·소리 시간), `line()`(" | " 구분), `lines()`(항목마다 한 줄)를 읽는다. `nullptr` 출처의 항목은 빠진다.
  전역 등록소(`profiler::record`)는 읽어 가는 전역 상태라 쓰지 않았고(아래 계층이 위를 부르지 않음), 화면 쪽 모으기라 `game/`(front 소스, 헤드리스 코어 밖)에 둔다. 함수 안 시간 재기는 지금처럼 Tracy `MEALKIT_ZONE`.
- **쓰는 곳**: sandbox 창 제목·F1 오버레이·자동 실행 `perf:` 로그(형식은 같고 `(worst N)`이 붙었다), 에디터 상태 표시줄 fps, 에디터 플레이 상태 줄의 틱·시뮬레이션 시간.
- **검증**: 단위 테스트 `frame_stats`(주기, 평균·최악, 출처 없는 글자, 주기 사이 초기화). check.ps1.
- **영향**: `kApiVersion`은 그대로(경계 함수는 바뀌지 않음). 기존 프로젝트의 `app/main.cpp`는 그대로 빌드되고, 같은 숫자를 쓰려면 sandbox처럼 `FrameStats`로 바꾼다.

## v0.4.20 — 1단계 진행 중 (2026-10-19)

### [x] 엔진 폴더 계층 정리 + 검사 (완료 2026-10-19, 로드맵 밖)
- **왜**: "핵심만 두고 나머지는 모듈화"를 검토했다. 하위 시스템(소리·UI·설정·연출·온라인)을 켜고 끄는 빌드 모듈은 얻는 것이 작아(출시 exe 2.4 MB / 예산 5 MB, 범용 에디터는 `mealkit.dll` 하나, `GameContext`가 서비스를 참조로 들고 있음) 만들지 않고, 대신 폴더 사이 include 방향을 한쪽으로 정하고 검사로 고정했다 (`docs/DECISIONS.md` 2026-10-19). 게임 기능 모듈화는 계획대로 4단계 소스 모듈.
- **순서** (`docs/CODING_MODEL.md` "엔진 폴더 사이 의존"): `core`·`platform` → `net` → `physics` → `sim` → `data` → `loc` → `scene` → `rhi`(2단계) → `render` → `asset` → `online` → `debug` → `input` → `audio` → `ui` → `feedback` → `settings` → `game` → `editor`. 폴더는 앞의 폴더만 include한다. `platform/<os>/` 구현 파일은 빼고 `check_platform.ps1`이 본다.
- **거꾸로였던 곳 고침**:
  - 표 파일 읽기 `mealkit/game/Tables.h` → 새 폴더 **`mealkit/data/Tables.h`**, 이름공간 `game::` → **`data::`**(`data::loadTable`, `data::resolveRows`, `data::TableText` …). 입력 바인딩·소리 이벤트·연출 표가 게임 경계(`game/`) 위로 올라가던 의존이 없어졌다. 게임 코드는 `mealkit::game::loadTable` 대신 `mealkit::data::loadTable`(sandbox `app/Headless.cpp`, 템플릿 CLAUDE.md, 문서 저장소 "Assets").
  - 게임패드 상태 `input::PadState`·`input::pad::A …` → `platform/Gamepad.h`의 **`PadState`·`pad::A …`**(이름공간 `mealkit`, 키보드 `key::`와 같은 자리). 플랫폼 계층이 입력 액션 헤더를 include하던 순환이 없어졌다.
- **검사**: `tools\check_layers.ps1` — 순서보다 뒤의 폴더를 include하거나 순서에 없는 폴더가 생기면 실패(check.ps1 "Engine layers", CI Windows 검사 단계). 지금 110개 파일, 폴더 사이 include 240개가 통과.
- **영향**: 게임 DLL 경계(`kApiVersion`)는 그대로. 기존 프로젝트의 게임 코드는 `game::loadTable` → `data::loadTable`, `#include "mealkit/game/Tables.h"` → `"mealkit/data/Tables.h"`로 고친다.

## v0.4.19 — 1단계 진행 중 (2026-10-19)

### [x] 매치 시작 데이터 — 플레이어별 로드아웃 (완료 2026-10-19)
- **왜**: 매치 시작 때 모두에게 보내는 값이 `seed`와 `matchData`(u32 하나)뿐이라, 각자 고른 캐릭터·장비를 매치에 가져갈 곳이 없었다.
- **네트워크** (`net/Lockstep.*`): `setLocalLoadout(바이트)`(매치 전, `kMaxLoadoutBytes` 128 — 8명이 시작 패킷 하나에 들어가게, 로드맵의 256에서 줄였다). 클라이언트는 로비 신호(접속 요청, 1초마다·바뀌면 바로)에 실어 보내고, 호스트가 `startMatch` 때 자기 것과 모두의 것을 확정해 시작 패킷에 싣는다. 혼자 플레이도 같은 길. `loadout(플레이어)`.
  시작 직전에 바꾼 값은 호스트가 받기 전이면 빠질 수 있다(로비 화면이 시작 전에 확정하게 한다 — 화면 흐름 항목).
- **게임 쪽** (`mealkit/game/Loadout.h`): `game::setLoadout(session, 값)` / `game::readLoadout(match, 플레이어, 값)` — 값은 `MEALKIT_STRUCT`(memcpy, 문자열 없음, 앞 4바이트 타입 해시). `game::MatchInfo`에 `loadouts`, `game::matchInfo(session)`.
  `GameInstance::lobby(session, options, userFolder)`(`kApiVersion` 19): 엔진(출시 exe, 에디터 플레이·참가 창)이 매치가 시작되기 전 매 프레임(혼자 플레이는 세션을 열기 직전) 부른다. 헤드리스 계약에 선택 함수 `botLoadout(player, session)`.
- **입력 기록 버전 3**: 머리에 플레이어마다 로드아웃 → 재생·CI가 같은 매치를 다시 만든다. 버전 1·2도 읽는다.
- **sandbox**: 정의 데이터 표 `tables/bodies.table`(`BodyKind`: normal·light·heavy, 이동 속도·질량 배율, 시뮬레이션 표라 콘텐츠 해시), 로드아웃 `Loadout{body}`(`app/Loadout.h`), 매치 시작이 `PlayerSlot::body`(상태 필드)에 복사하고 이동·Pawn 질량이 읽는다(AI는 첫 행). 사람은 세이브의 `body`, 봇은 플레이어마다 다른 몸.
  상태 필드가 늘고 봇 몸이 달라져 고정 기록을 다시 만들었다(`arena_4p_seed7.rec`, 최종 해시 `ccfe93aa4c49525b`).
- **결정론**: 로드아웃은 상태 필드로 복사되어 상태 해시·스냅샷에 들어간다. 잘못된 값(가지지 않은 아이템)은 막지 못한다(서버 검증은 보류 표). 없는 행 번호는 첫 행으로.
- **검증**: `command_channel` 테스트에 UDP 루프백 로드아웃(양쪽이 같은 값, 다른 타입은 읽지 않음)과 기록 머리 왕복. `mp_test` 헤드리스 1+3 PASS.

## v0.4.18 — 1단계 진행 중 (2026-10-19)

### [x] 세이브 — 진행 데이터 (완료 2026-10-19)
- **왜**: 해금·통계·인벤토리처럼 매치 밖에 남는 데이터를 게임마다 다른 방식으로 저장하면 형식 변경·클라우드를 매번 다시 만든다.
- **방식** (`mealkit/game/Save.h`, 엔진 코어): 게임의 `MEALKIT_STRUCT` 하나를 `game::writeSave(data, ctx.userFolder, "이름", 버전)` / `readSave(...)`로. 파일은 사용자 폴더의 `saves/<이름>.save`, 첫 줄 `# MealKit save (타입)`, `version N`, 그다음 필드 텍스트("경로 값").
  읽을 때 모르는 필드는 버리고 없는 필드는 기본값. 파일 버전이 낮으면 `migrate(파일 버전, 줄)`이 줄을 고칠 기회를 받는다(이름이 바뀐 필드 옮기기). 높으면 경고. 쓰기는 임시 파일 후 바꿔 넣기(`writeTextFile`), 직전 파일은 `.bak`.
- **`GameContext::userFolder`** (`kApiVersion` 18): 출시 exe·에디터 플레이는 `game::userFolder(설정)`, 자동 실행(`--exit-tick`)·참가 창은 빈 문자열 → 저장하지 않는다(시험이 PC 상태에 기대지 않게).
- **sandbox**: `app/SaveData.h`(`SaveData`: 시작한 매치 수, 내가 떨어진 횟수, 내 밀치기 수, 플레이 시간), 매치 시작·내가 떨어질 때·30초마다 저장, HUD에 "기록: N번 떨어짐 · M분 플레이".
- **넣지 않은 것 (로드맵대로)**: 계정 서버·DB(보류 표). 매치 안 상태의 중간 저장(`WorldSnapshot` 파일)은 3단계 중간 참가와 같은 데이터라 그때. 클라우드는 Steam Auto-Cloud(Steamworks에서 `saves/` 지정, 코드 없음), 다른 플랫폼은 3단계 EOS Player Data Storage.
- **검증**: 단위 테스트 `save_files`(쓰기·`.bak`·버전 1 → 2 migrate로 필드 이름 옮기기·새 필드 기본값·없는 파일·빈 폴더면 저장 안 함).

## v0.4.17 — 1단계 진행 중 (2026-10-19)

### [x] 설정 시스템 + 표준 설정 화면 (완료 2026-10-19)
- **왜**: 거의 모든 게임이 화면·소리·키 설정을 같은 모양으로 다시 만든다. 엔진이 기본 화면을 주고 게임은 모양만 바꾸게 한다.
- **스키마** (`mealkit/settings/Settings.h`, 필드 선언): 화면(창 모드 창·테두리 없는 창·전체 화면, 창 크기, 수직 동기, 프레임 제한, 렌더 배율, 그래픽 품질, 시야각, 밝기), 소리(전체·배경음·효과음·인터페이스·음성 볼륨, 창이 뒤에 있으면 소리 끄기), 조작(마우스·게임패드 감도, Y축 반전), 게임(언어, 인터페이스 크기, 자막).
  파일은 사용자 폴더의 `settings.ini`(`game::userFolder`, "경로 값" 줄, 콘텐츠 해시와 무관).
- **`settings::Service`** (`GameContext::settings`, `kApiVersion` 17): 엔진(출시 exe, 에디터 플레이, 참가 창)이 하나 만들어 읽고 적용한다 — 창 모드·크기(`Window::setMode`, 새 플랫폼 기능, 모니터 해상도 목록 `displaySizes`), 수직 동기, 프레임 제한, 오디오 버스 볼륨, 포커스 음소거, 언어(`Strings::load`), UI 배율, 감도·반전(`Actions::setLookSettings`: `Delta2` 액션의 마우스·패드 몫, 양자화 전 화면 쪽).
  게임은 `values()`로 읽는다(sandbox: 시야각). `open()`/`close()`, 어느 `.ui`든 버튼 액션 `settings.open`이 표준 화면을 연다.
  전체 화면은 모니터 크기의 테두리 없는 창이다(플립 모델 스왑 체인이라 독점 전체 화면 없이 지연이 같다). 렌더 배율·그래픽 품질·밝기는 지금 저장만 한다 — 2단계 동적 해상도·포스트 프로세싱에서 쓴다.
- **표준 설정 화면** (`assets/ui/settings.ui`, 엔진 뷰 모델 `SettingsVm`): 탭 목록과 줄 목록을 스키마에서 채운다(토글·슬라이더·선택 `<` `>`·키). 게임은 디자인만 바꾸고 바인딩·`settings.*` 액션은 그대로 둔다. 키보드·게임패드로 조작(`navigation`), Esc·B는 닫기.
  해상도·창 모드를 바꾸면 "유지할까요? N초 뒤 되돌림" 창을 띄우고 15초 안에 [유지]를 누르지 않으면 되돌린다. [이 탭 기본값].
  키 탭: 버튼 액션(`ui.*` 제외)마다 줄, 누르면 다음 키로 바꾸고(Esc 취소) 사용자 폴더의 `input.bindings`에 저장, 다른 동작과 같은 키면 경고, [이 탭 기본값]이 모든 키를 되돌린다. 글자는 문자열 표의 `settings.*`·`action.<id>`·`language.<코드>`.
- **자동 실행**(`--exit-tick`)은 기본값으로 돌고 저장하지 않는다(이 PC의 설정이 시험 결과를 바꾸지 않게). `--size`·`--vsync`·`--fps`를 주면 그 실행은 화면 항목보다 명령줄. 에디터 플레이는 에디터 창의 모드를 바꾸지 않고, 참가 창은 저장하지 않는다. `--settings`는 설정 화면을 연 채로 시작.
- **SimCheck**: `sim/`에서 `mealkit/input/`, `mealkit/loc/`, `mealkit/feedback/` include도 막는다(`mealkit/settings/`는 원래 막혀 있었다).
- **sandbox**: HUD [설정 (F10)] 버튼(`settings.open`), 입력 액션 `menu.settings`(F10, 패드 Start), 시야각은 플레이어 설정, 설정 화면이 열려 있으면 Esc가 게임을 끄지 않는다.
- **검증**: 단위 테스트 `settings_service`(화면을 열고 토글을 누르면 값이 바뀌어 파일에 저장, 다시 읽으면 그 값, 닫기). `sandbox.exe --settings` 캡처.
- **남은 것**: 게임이 더하는 설정 항목(`GameClass::SettingsType`, `game.` 접두사), 축 액션(이동 4키)의 키 바꾸기, 버튼 안내 아이콘, 게임패드 스틱으로 메뉴 이동.

## v0.4.16 — 1단계 진행 중 (2026-10-19)

### [x] 게임 화면 UI 1부: `.ui` 런타임 + 미리보기 패널 (완료 2026-10-19)
- **왜**: 점수·로비 상태를 창 제목으로만 보여 줬다. 샘플의 HUD·메뉴·설정·화면 흐름을 만들 UI 런타임이 필요하다. 로드맵의 순서대로 Penpot 플러그인(2부)보다 런타임을 먼저 만들었고, 지금 화면은 `.ui`를 손으로 쓴다.
- **`.ui` 형식** (`mealkit/ui/Document.*`, `assets/ui/*.ui`): 한 줄에 속성 하나, `board 이름 1920 1080`(뿌리, 기준 해상도의 논리 단위) / `frame` / `text` / `image` / `use 이름 컴포넌트.ui`(재사용, `set 자식 속성 값`으로 덮기)로 열고 `end`로 닫는다.
  배치: `pos`, `size 숫자|hug|fill`, `min`/`max`, `margin`, `absolute`, `constraints left|right|leftright|center|scale top|...`, `rotation`, `opacity`, `hidden`, `clip`. Flex: `layout row|column [reverse] [wrap]`, `gap`, `padding`, `justify start|center|end|space-between|space-around|space-evenly`, `align`, `alignContent`, `alignSelf`.
  그리기: `fill #RRGGBB[AA]|$토큰|linear …|radial …`, `stroke`, `radius`(모서리별), `shadow dx dy blur spread 색`(여러 개), 이미지 `src`·`slice`(9-slice)·`tint`. 글자: `key 문자열키` 또는 `content 글자`, `font`, `fontSize`, `color`, `letterSpacing`, `lineHeight`, `textAlign`, `verticalAlign`, `underline`, `strike`, `transform`, `wrap word|syllable|none`, `overflow ellipsis`, `maxLines`, `span 번호 속성…`(번호 태그 `<1>…</1>`의 스타일).
  바인딩: 글자 안 `{필드}`·`{필드:n0|f1|time|pct}`, `visible [!]필드`, `disabled`, `widthRatio`(체력 막대), `colorField`. 위젯: `button 액션`, `toggle 필드`, `slider 필드 최소 최대 간격`, `select 필드 키1,키2`(글자 안 `{option}`), `scroll [horizontal]`, `list 필드`(첫 자식이 반복 템플릿, 나머지 자식은 예시). 상태: `on hover|pressed|focus|disabled|checked 속성…`, `transition 초`. 보드 줄: `navigation`(키보드·게임패드 포커스), `firstFocus`, `back 액션`, `focusUp/Down/Left/Right 이름`.
  테마 `assets/ui/theme.ui`(`color 이름 #…`, `number 이름 값`, 색 토큰은 그릴 때·숫자 토큰은 읽을 때). 첫 줄이 `# MealKit UI exported from Penpot`이면 플러그인이 낸 파일로 표시한다(2부). 오류는 `파일(줄): …`.
- **런타임** (`ui::System`, `mealkit/ui/Ui.*`, `GameContext::ui`, `kApiVersion` 16): `open`·`openWorld`(월드 위 템플릿)·`close`·`bind(화면, 뷰 모델)`·`actions()`·`drawAt(템플릿, 3D 위치, 뷰 모델)`. 엔진(출시 exe, 에디터 플레이, 참가 창)이 매 프레임 `update`(게임 update 전: 마우스·포커스, UI 위의 마우스·포커스된 위젯은 게임이 받지 않는다)와 `render`(3D 다음, 디버그 UI 전)를 부른다.
  `.ui`를 읽으면 깊이 우선 노드 배열이 되고, 목록은 뷰 모델 개수만큼 펼친다(개수가 바뀌면 다시). 배율 = `min(가로/보드 가로, 세로/보드 세로) × UI 배율`, 뿌리는 화면 전체로 늘고 자식은 제약으로 붙는다. Flex는 CSS 규칙(채우기 = 남는 자리 나누기, 교차축 늘리기, 줄바꿈, 여러 줄 배분).
  글자: 구간 경계를 넘어 한 번에 줄을 나눈다(한글·한자·가나는 음절 단위, `word`는 공백에서만), 커닝, 줄 높이·기준선은 CSS의 반 행간, 말줄임(줄 수 제한·고정 높이), 밑줄·취소선. 글꼴은 `assets/fonts`에서 파일 이름으로, 글꼴에 없는 글자는 다른 글꼴에서 찾는다.
  조작: 마우스(올림·눌림·클릭, 슬라이더 끌기, 휠 스크롤), 입력 액션 `ui.up/down/left/right/confirm/back`(`navigation`이 있는 가장 위 화면만 — HUD가 이동 키를 가져가지 않게): 방향의 가장 가까운 위젯으로 포커스, 슬라이더·선택 상자는 좌우로 값, 마우스를 움직이면 포커스 표시를 숨긴다. 입력 위젯만 자기 뷰 모델 필드에 값을 쓰고 액션(지정 이름 또는 `changed`, 바뀐 필드)을 낸다.
  파일(.ui·테마·글꼴·이미지)이 바뀌면 0.25초 안에 다시 읽는다. 진단: `info`(노드 수, 파일·바인딩 오류), `boxes`(노드별 상자 — 2부 배치 비교가 쓴다), `layout`(그리지 않고 배치만), `press`(시험용).
- **그리기** (`shaders/Ui.hlsl`, `Renderer::drawUi`·`updateUiAtlas`): 사각형 인스턴스 112바이트 하나가 모서리별 둥근 사각형·단색·선형·원형 그라디언트·안쪽 테두리·흐린 그림자·회전·이미지·SDF 글자를 픽셀 셰이더 SDF로 그린다. 정점 버퍼 없이 화면 전체 UI가 드로우 콜 1회, 미리 곱한 알파, 감마 변환 없는 오버레이 뷰(색은 sRGB 그대로), 백버퍼 원래 해상도.
  아틀라스 두 장(글자 R8, 이미지 RGBA8, 2048²)은 바뀐 영역만 프레임 업로드 링으로 올린다. 아틀라스는 SRV 표 두 칸(SM 6.0) — SM 6.6 bindless는 2단계 RHI에서.
- **글꼴·이미지** (`ui::Atlas`): 쓰는 글자만 실행 중에 SDF로 굽는다(40 em픽셀, 여백 6). **SDF는 엔진이 직접 계산**: `stbtt_GetGlyphSDF`가 3차 곡선(CFF `.otf`, Pretendard)을 빼먹어 둥근 획(ㅇ, 0)이 사라져서, stb_truetype의 윤곽선을 선분으로 펴 거리·감김 수로 계산한다(`docs/DECISIONS.md` 2026-10-19). 이미지는 stb_image(PNG만). 아틀라스가 차면 비우고 다시 굽는다.
- **뷰 모델** (`core/Reflect.h`): `MEALKIT_VIEWMODEL(이름)`(모듈 목록 `reflect::moduleViewModels()`, `GameModule::viewModels`), `MEALKIT_LIST(행 타입, 이름, 최대, 설명)` = 배열 + `이름Count`.
- **에디터 [UI] 패널** (`editor/UiPanel.*`, [보기] > [UI], `--ui <파일>`): `assets/ui`의 화면 목록, 고른 화면을 편집 모드의 씬 뷰 위에 그린다. 해상도(16:9 1080p·720p, 21:9, 4:3, 세로) 비율로 맞춘 테두리, UI 배율, 언어 전환(글자 넘침), 포커스 표시, 게임 뷰 모델을 골라 가짜 값을 필드 위젯으로, 노드 수(예산 500), 파일·바인딩 오류 목록.
- **sandbox**: HUD(`assets/ui/hud.ui`: 플레이어 목록 — 색·이름·"나"·떨어진 횟수, 경기 시간, 밀치기 준비 막대, [충격파] 버튼 → `debug.shockwave` 액션 → 확정 명령, 리스폰 안내)와 캐릭터 위 이름표(`nametag.ui`, 월드 위 UI), 테마, 문자열, 입력 바인딩 `ui.*`. 뷰 모델 `HudVm`·`NameTagVm`·`PlayerRowVm`(`view/Hud.*`), 화면용 튜닝 `nameTagHeight`.
- **고친 것**: 에디터 여러 명 플레이에서 참가 창이 가끔 못 들어오던 문제 — 에디터 프로세스마다 시작할 때 `build\Editor\bin\hot\`을 비워, 동시에 뜬 참가 창이 서로 막 복사한 게임 DLL을 지웠다(`LoadLibrary failed (2)`). 이제 자기 복사본과 1분 넘게 된 남의 복사본만 지운다(`GameModuleHost::load`).
- **엔진**: `Input::mouseX/mouseY/mouseMoved`(커서 위치), 단위 테스트가 창 있는 빌드에서 UI 모듈을 링크한다(`MEALKIT_TEST_UI`).
- **검증**: 단위 테스트 `ui_layout`(Flex 여백·간격·정렬·채우기, 제약 오른쪽·가운데·양쪽, 목록 개수·예시 자식 빼기, 보이기 바인딩, 토글이 필드에 쓰고 `changed`, 오류 줄 번호). 게임 캡처(1080p·720p)와 에디터 플레이·미리보기 캡처. 상태 해시 그대로.
- **남은 것 (1부 안)**: Grid 레이아웃(`layout grid`는 아직 읽기 오류 — 2부 지원 범위와 같이), 글자 입력 위젯(IME), 회전은 노드 자신의 사각형만(자식·글자는 돌지 않음), 지금은 매 프레임 다시 배치한다(화면당 수십 µs — "바뀐 노드만"은 화면이 커지면), 게임패드 스틱으로 메뉴 이동(지금은 십자키), 버튼 안내 아이콘(마지막 기기). 2부는 ROADMAP의 다음 항목.

## v0.4.15 — 1단계 진행 중 (2026-10-18)

### [x] 로컬라이제이션 문자열 표 (완료 2026-10-18)
- **왜**: UI를 만들면서 글자를 코드에 바로 쓰면 나중에 다른 언어를 넣을 때 모든 글자를 찾아 바꿔야 한다. 게임 화면 UI보다 먼저 둔다.
- **형식**: `assets/strings/<언어>.txt`, 한 줄에 `키 = 문자열`(키는 영문자·숫자·`._-`, 첫 글자 영문자), `#` 주석, `\n` 줄바꿈, `{이름}` 자리, 여러 스타일 구간은 번호 태그 `<1>…</1>`(UI가 해석하고 표는 그대로 돌려준다). UTF-8 BOM도 읽는다.
- **`loc::Strings`** (`mealkit/loc/Strings.h`, 엔진 코어): `GameContext::strings`(`kApiVersion` 15). `get(키)`, `format(키, {{"이름", 값}})`, `has`, `languages()`(설정 화면의 언어 목록).
  고른 언어에 없는 키는 기본 언어 표에서, 거기에도 없으면 키 이름을 보여 주고 키마다 경고 한 번. 파일은 0.25초마다 시각을 보고 다시 읽는다.
  기본 언어는 프로젝트 설정 `language`(기본 `ko`), 출시 exe는 `--lang <코드>`로 바꾼다(설정 화면이 생기면 플레이어가 고른 언어). 에디터 플레이·참가 창은 기본 언어.
- **빌드 때 글자 검사** (`engine/tools/StringCheck.cpp` → `mealkit_stringcheck`, stb_truetype): `mealkit_add_game`이 게임 exe·게임 DLL 전에 `assets/strings/*.txt`의 모든 글자가 `assets/fonts`(하위 폴더 포함 `.ttf`·`.otf`·`.ttc`)의 글꼴 중 하나에 있는지 본다.
  없으면 `파일(줄): error STR001: U+XXXX ...`로 빌드가 멈춘다. 글꼴이 하나도 없으면 검사하지 않는다(아직 게임 UI가 없는 프로젝트). 에디터 빌드의 게임 프로젝트에도 내보낸다(`MealKitTargets.cmake`).
- **글꼴**: `assets/fonts/Pretendard-Medium.otf`(Pretendard 1.3.9, SIL OFL 1.1, 1.5 MB)와 라이선스 `Pretendard-OFL.txt`(출시 고지문에 저절로 들어간다, v0.4.6 규칙). `.otf`·`.ttf`·`.ttc`는 `.gitattributes`에서 바이너리.
- **sandbox**: 플레이어에게 보이는 창 제목(게임 이름, 로비·연결·대기·끝남 문구)을 `ko.txt`·`en.txt`로 옮겼다. 성능 숫자는 개발용이라 영어 그대로. 끝남 이유는 아직 엔진 영어 글자 — "버전 표시와 접속 거부 이유" 항목에서 이유 코드로 바꾼다.
- **엔진**: `listFiles(폴더, 확장자)`(`platform/Paths.h`).
- **검증**: 단위 테스트 `strings_table`(읽기·오류 줄·기본 언어 대체·없는 키·`format`·언어 목록·키 규칙). 글꼴에 없는 글자(☃, 한자)를 넣으면 빌드가 `STR001`로 멈춘다. `--lang fr`은 경고 후 기본 언어.
- **남은 것**: Penpot 플러그인의 키 붙이기·태그 내보내기는 게임 화면 UI 항목, 언어 고르기는 설정 화면 항목.

## v0.4.14 — 1단계 진행 중 (2026-10-18)

### [x] 디버그 UI (Dear ImGui) (완료 2026-10-18)
- **왜**: 성능·치트·시뮬레이션 상태 같은 개발용 화면은 프로그래머가 코드로 빨리 만든다. 게임 UI(Penpot)와 섞지 않는다.
- **게임 쪽**: `GameInstance::debugUi(GameContext&)`(기본은 빈 함수, `kApiVersion` 14). 엔진이 ImGui 프레임 안에서 부르고, 게임은 몸을 `#if MEALKIT_DEBUG_UI`로 감싸 `<imgui.h>`를 바로 쓴다.
  시뮬레이션을 바꾸는 버튼은 상태를 고치지 않고 확정 명령을 보낸다(sandbox [Shockwave] 버튼 = `DebugShockwave` 명령).
- **개발용 게임 exe** (`build\Release`·`Debug`·`Profile`): **F1**로 켜고 끄는 오버레이(처음 켤 때 ImGui를 만든다 — 글꼴 읽기). 엔진 창 "MealKit"(창 제목의 성능 숫자, 왼쪽 아래)과 게임의 창들.
  디버그 창 위에서는 게임이 키·마우스를 받지 않는다(`WantCaptureKeyboard/Mouse` → `keyboardAllowed/mouseAllowed`). `--debug-ui`로 켠 채로 시작(캡처·자동 시험).
- **에디터**: 플레이 중 매 프레임 `debugUi`를 에디터의 ImGui 프레임 안에서 부른다. ImGui를 `mealkit.dll` 안에 넣어(내보내기 def에도) 에디터와 게임 DLL이 ImGui 하나·컨텍스트 하나를 쓴다 — 로드맵에 적었던 `SetCurrentContext` 넘기기가 필요 없다. 참가 창에는 아직 없다.
- **출시 빌드**: `build.bat Export`가 **`build\Export`**에서 `-DMEALKIT_DEBUG_UI=OFF`로 따로 빌드한다(`docs/DECISIONS.md` 2026-10-18). ImGui 코드·글자가 출시 exe에 없다(개발 exe 2583 KB, 출시 exe 2093 KB). 처음 Export는 엔진을 한 번 더 빌드한다.
- **엔진**: ImGui 연결 층을 `editor/`에서 `mealkit/debug/ImGuiLayer.*`로 옮겼다(에디터·오버레이가 같이, 빈 ini 경로는 창 위치를 저장하지 않음). ImGui는 OBJECT 라이브러리로 따로 컴파일해(경고·UNICODE 옵션 분리) 엔진 타깃에 넣고, `MEALKIT_DEBUG_UI=1`·imgui 경로를 게임에 넘긴다. ImGuizmo는 에디터만.
- **sandbox**: "Sandbox" 창(틱, 플레이어, Pawn·상자·움직이는 물체 수, 플레이어별 떨어진 횟수, [Shockwave]). 충격파 키는 에디터의 F6(일시정지)과 겹쳐 **F8**로 옮겼다.
- **검증**: 에디터 플레이 캡처와 `sandbox.exe --debug-ui` 캡처에서 창이 보인다. 출시 exe에 `ImGui` 글자가 없다. 상태 해시 그대로.
- **남은 것**: 참가 창의 오버레이, 4단계 "디버그 그리기 API"·"성능 패널과 예산 경고"가 이 위에 놓인다.

## v0.4.13 — 1단계 진행 중 (2026-10-18)

### [x] 확정 명령 채널 (완료 2026-10-18)
- **왜**: 락스텝으로 보내는 것은 틱당 플레이어 입력 8바이트뿐이라, 치트·입력 지연 변경·일시정지 투표·매치 중 설정 변경처럼 가끔 있고 크기가 제각각인 명령을 보낼 길이 없었다.
- **네트워크** (`net/Lockstep.*`): `submitCommand(data, size)`(1..`kMaxCommandBytes` 200바이트). 호스트는 받은 명령을 기다리게 두었다가 **확정하는 틱**(모든 입력이 모인 순간)에 싣는다:
  플레이어 번호 → 받은 순서, 틱당 `kCommandBytesPerTick` 256바이트(명령마다 머리 2바이트)까지, 넘치면 그 플레이어의 나머지는 다음 틱(순서 유지).
  명령은 `net::TickInputs`(`commandBytes` + `commands`)에 들어가 입력 묶음과 같이 배포된다(입력과 같은 중복 전송). 클라이언트 → 호스트는 번호를 붙여 호스트가 확인(묶음 머리의 `commandAck`)할 때까지 매 입력 패킷에 다시 싣고, 호스트는 번호로 중복을 버린다.
  묶음은 명령이 있는 틱만큼 패킷(1200바이트)에 들어가는 틱 수를 줄인다. 혼자 플레이도 같은 길(다음 틱에 실린다).
- **게임 쪽** (`mealkit/sim/Commands.h`): 명령 타입은 게임 `sim/`의 `MEALKIT_STRUCT`(memcpy로 보내므로 문자열·컨테이너 없음, `static_assert`), 타입 이름의 해시 4바이트로 구분.
  화면 쪽은 `sim::sendCommand(ctx.session, 명령)`, `World::step()` 맨 앞에서 `sim::forEachCommand(tickInputs, ...)` + `c.read(out)`/`c.is<T>()`, `c.player`는 보낸 플레이어.
- **기록**: 명령이 틱 입력 안에 있으므로 되감기·그림자 시뮬레이션·`--record-inputs`에 저절로 들어간다. 입력 기록 파일은 **버전 2**(틱마다 u16 명령 바이트 수 + 바이트), 버전 1도 읽는다.
  헤드리스 계약에 선택 함수 `botCommands(tick, player, session)`(봇의 명령, `GameSession`의 `--bot`도 같은 함수).
- **sandbox**: 디버그 명령 `DebugShockwave`(`sim/Commands.*`: 보낸 플레이어 캐릭터 주변 상자를 밀치기 힘으로 밀어냄, `Push` 이벤트), 액션 `debug.shockwave`(F6), 봇이 300틱마다 하나씩 보낸다.
  봇 결과가 바뀌어 고정 기록 `samples/sandbox/replays/arena_4p_seed7.rec`를 다시 만들었다(이제 명령 12개가 들어 있어 CI가 모든 OS에서 명령 처리도 비교한다. 최종 해시 `d5e47a38258e973e`).
- **검증**: 단위 테스트 `command_channel`(혼자: 다음 틱, 순서, 틱당 크기 넘침, 기록 왕복 / UDP 루프백 호스트 + 클라이언트: 양쪽이 같은 틱에 같은 명령). `mp_test` 1+3 PASS, 예전 버전 1 기록도 그대로 `REPLAY ok`.
- **남은 것**: 엔진 예약 명령(입력 지연 변경)은 "입력 지연 자동 조절" 항목에서. 대기열이 차면 `false`(호스트 256개, 클라이언트 64개).

## v0.4.12 — 1단계 진행 중 (2026-10-18)

### [x] 게임패드 (완료 2026-10-18)
- **방식**: 플랫폼 계층 `mealkit::Gamepads`(`platform/Gamepad.h`, Windows는 `win32/Gamepad.cpp`의 XInput 1.4, `xinput` 링크): Xbox·XInput 호환 패드 4슬롯. 연결된 슬롯은 매 프레임, 연결되지 않은 슬롯은 1초에 한 번만 `XInputGetState`(빈 슬롯은 약 14 µs, `docs/DECISIONS.md` 2026-10-18). 연결·해제는 로그에.
  스틱은 -1..1(y는 위가 +), 트리거 0..1, 버튼은 `input::pad::*` 비트로 `input::PadState`에 담는다. 데드존은 입력 액션 쪽(행마다 `deadzone`, 스틱은 원형).
- **입력 액션의 바인딩 소스**: 엔진(출시 exe, 에디터 플레이, 참가 창)이 매 프레임 `first()`(연결된 첫 패드)를 `Actions::update`에 넘긴다. 창에 포커스가 없을 때(에디터는 게임 뷰가 키보드를 받지 않을 때)는 넘기지 않는다.
  `pad` 열에 `-이름`은 음수 쪽: `pad DPadUp -DPadDown`처럼 버튼 둘로 축 하나. 트리거를 버튼으로 쓰면 0.5 이상이 눌림.
- **sandbox**: `move` 왼쪽 스틱, `jump` A, `push` 오른쪽 트리거·X, 새 액션 `turn`(`Delta2`, 오른쪽 스틱, `rate 500` — 마우스 픽셀과 같은 단위라 `orbitSensitivity`를 같이 쓴다)은 누르지 않고 바로 카메라를 돌리고, 마우스 `look`은 오른쪽 버튼을 누르는 동안만. `zoom`에 십자 위아래.
- **검증**: 단위 테스트 `input_actions`에 패드 데드존·`-이름`. 출시 exe 크기 변화 없음(시스템 DLL). **실제 패드로는 시험하지 않았다**(이 PC에 패드 없음) — 패드를 꽂으면 로그에 `Gamepad 0 connected`가 나온다.
- **남은 것**: PlayStation·Switch 패드(Windows.Gaming.Input/RawInput), 진동, 여러 로컬 플레이어(지금은 첫 패드만). 다른 플랫폼은 5단계.

## v0.4.11 — 1단계 진행 중 (2026-10-18)

### [x] 입력 액션 매핑 (완료 2026-10-18)
- **왜**: 게임 코드가 `'W'`, `Space`, 마우스 버튼을 직접 확인해, 키 바꾸기·게임패드를 따로 처리해야 했다.
- **액션과 바인딩은 데이터**: 엔진 표 `assets/input.bindings`(엔진 행 타입 `input::Action`, 화면 전용 표 — 에디터 [표] 패널, CSV). 행 id = 액션 이름.
  열: 종류(`MEALKIT_ENUM ActionKind`: `Button`, `Axis` -1..1, `Axis2` 길이 1 이하, `Delta`·`Delta2` 이번 프레임 변화량), 플레이어에게 보이는 이름, 키(`Axis`는 "음수 양수" 짝, `Axis2`는 "위 아래 왼쪽 오른쪽" 넷씩, `-`는 빈 자리),
  마우스(`Left Right Middle WheelUp WheelDown`, 값은 `Wheel Move MoveX MoveY` — 픽셀·휠 칸 그대로), 게임패드(`A B X Y ... LeftStick RightStick DPad LeftTrigger`), `rate`(변화량 액션에서 키·스틱의 초당 양), 데드존.
  모르는 이름은 읽을 때 경고. 파일은 0.25초마다 시각을 보고 다시 읽는다(플레이 중에 [표] 패널에서 고치면 바로).
- **`input::Actions`** (`mealkit/input/Actions.h`, 엔진 코어 — 헤드리스 단위 테스트도 쓴다): 엔진(출시 exe, 에디터 플레이, 참가 창)이 창마다 하나 만들어 `GameContext::actions`로 넘기고(`kApiVersion` 13), 매 프레임 게임 `update` 전에 갱신한다.
  게임은 `down/pressed/released/axis/axis2("이름")`만 읽는다. 키보드·마우스를 받지 않는 동안(에디터 게임 뷰 밖, 창 포커스 없음)은 놓은 상태. 여러 바인딩은 더한다(버튼은 OR, `Axis2`는 길이 1로 자른다). 없는 액션은 0과 이름마다 한 번 경고. `lastDevice()`(버튼 표시용).
  게임패드 상태(`input::PadState`)를 받는 자리와 데드존(스틱은 원형, 밖을 0..1로 다시 편다)까지 이 항목에서 만들었다. 실제 패드 읽기는 다음 항목(XInput).
- **플레이어가 바꾼 키**: `setBinding`/`resetBinding`/`saveUser`가 사용자 폴더(`game::userFolder(설정)` = 사용자 앱 데이터 폴더/<게임 이름>)의 `input.bindings`에 프로젝트와 다른 행의 바인딩 필드만 저장하고, 읽을 때 프로젝트 값 위에 덮는다(종류·이름은 게임 것). 콘텐츠 해시와 무관. 바꾸는 화면은 "설정 시스템 + 표준 설정 화면" 항목.
- **sandbox**: 액션 `move`(WASD·화살표, 왼쪽 스틱), `jump`, `push`, `orbit`(오른쪽 버튼을 누르는 동안 카메라 돌리기), `look`(`Delta2`, 마우스 이동), `zoom`(`Delta`, 휠). `view/LocalInput`과 `OrbitCamera::control`이 액션만 읽고, 이동은 `axis2("move")` 길이만큼(스틱은 기울인 만큼) 카메라 기준 방향으로 양자화한다.
  `Vec2`(`core/Math.h`)를 더했다. 엔진·디버그 키(Esc, V, F12, 로비의 Enter·I)는 `app/main.cpp`에 남겨 두었다 — 화면 흐름 템플릿 항목에서 UI 액션으로 옮긴다.
- **검증**: 단위 테스트 `input_actions`(대각선 길이, 허용 안 됨, 눌림·뗌, 변화량, 데드존, 사용자 파일 덮기·되돌리기·이름 표). 상태 해시 그대로(입력 바이트 형식 그대로, 고정 기록 `REPLAY ok`).
- **기존 프로젝트**: `assets/input.bindings`가 없으면 액션이 없다는 경고가 나고 조작이 안 된다. 예제에서 그 파일을 복사하고 `upgrade_project.ps1 -Source`(또는 게임 코드를 액션으로)로 올린다.

## v0.4.10 — 1단계 진행 중 (2026-10-18)

### [x] 이벤트 → 연출 표 (타격감을 데이터로) (완료 2026-10-18)
- **왜**: 틱 이벤트에 소리·흔들림을 붙이려면 `view/` 코드를 써야 했다(오디오 항목의 `view/Sounds.cpp`도 그랬다). 사운드·VFX 담당이 프로그래머 없이 타격감을 만지게 한다.
- **`MEALKIT_ENUM(Name, Underlying, A, B, C)`** (`core/Reflect.h`): 값 이름이 있는 enum(0부터 차례로). 파일·에디터에는 이름(예전 숫자도 읽는다), 메모리·해시는 정수 그대로. `ValueInfo::enumNames`, `reflect::enumName(v)`, 에디터는 이름 고르기. 연출 표가 이벤트 종류를 이름으로 고르는 데 쓴다.
- **연출 표** `assets/feedback.table` (엔진 행 타입 `feedback::Reaction`): 행 id = 이벤트 종류 이름(게임 이벤트 타입의 첫 `MEALKIT_ENUM` 필드), 같은 종류에 여러 줄은 `종류.이름`(예: `Fall`, `Fall.local`).
  열: 누구의 이벤트(`any`/`local`/`others`, 이벤트의 `player` 필드와 이 창의 플레이어), 세기를 읽을 필드와 최대 세기 값, 위치를 읽을 필드(비우면 위치 없음), 소리 이벤트와 세기를 넣을 소리 파라미터,
  카메라 흔들림(크기·시간·빠르기·사라지는 거리), 화면 번쩍임(세기·시간·색), 화면 정지(초).
- **`feedback::Player`** (`mealkit/feedback/Feedback.h`, `GameContext::feedback`, `kApiVersion` 12): 게임은 틱 이벤트마다 `ctx.feedback.trigger(event, localPlayer)`만 부른다(이벤트 타입은 `MEALKIT_STRUCT`, 필드는 경로로 읽는다).
  `cameraShake(focus)`(축마다 다른 주파수의 사인 합, 시간·거리 감쇠) — 게임 카메라가 초점에 더한다. `screenTint`(엔진이 `FrameParams::tintColor/tintAmount`로: 셰이더와 배경 지우기 색이 그 색으로 섞인다). `hitStop()`.
  표는 0.25초마다 파일 시각을 보고 다시 읽는다 — 플레이 중에 고치면 바로 보인다. 출시 exe·에디터·참가 창이 하나씩 만든다(에디터는 소리 엔진과 같이).
- **화면 정지는 화면만**: 시뮬레이션은 계속 돈다(락스텝). sandbox는 정지가 시작된 순간의 두 틱 상태(`WorldState` 사본)와 보간 비율을 붙잡아 그 그림을 그리고, 화면 시계(효과)를 멈춘다. 끝나면 지금 상태로 바로 돌아온다.
- **에디터 [연출] 패널** (`editor/FeedbackPanel.*`, [보기] > [연출], `--feedback`): 왼쪽은 게임의 `EventType`(`GameModule::eventType`) 종류 이름과 행 수, 오른쪽은 고른 종류의 행들(필드 위젯, 삭제, 추가 — 둘째 줄부터 접미사), 이벤트 필드 목록(세기·위치에 고를 수 있는 것).
  고치면 0.3초에 한 번까지 저장하고 게임이 다시 읽는다. [미리보기]는 그 종류의 행을 지금 한 번 낸다(편집 중에도 소리와 번쩍임이 보인다). 이 표는 [표] 패널에는 나오지 않는다(둘이 같은 파일을 덮어쓰지 않게).
- **sandbox**: `GameEvent`를 `MEALKIT_STRUCT`(종류 `MEALKIT_ENUM(EventKind, ...)`, player, position, value), 만들기는 `makeEvent()`(기반 클래스가 있어 중괄호 초기화가 안 된다). `using EventType = GameEvent`.
  `view/Sounds.*`를 지우고 연출 표로: 밀치기(소리 + 흔들림, 내 밀치기는 번쩍임 + 화면 정지 0.05초), 점프대(소리 + 흔들림), 스폰·점프(소리), 낙사(소리, 내가 떨어지면 붉은 번쩍임), 충돌(세기 = 속도/15로 소리 파라미터와 흔들림).
  충격파 고리는 아직 코드(`view/Effects.cpp`) — 2단계 파티클에서 연출 표의 효과로 옮긴다.
- **검증**: 단위 테스트 `reflect_enum`. 상태 해시 그대로(이벤트는 해시 밖, 고정 기록 `REPLAY ok`). 에디터 캡처에서 [연출] 패널(종류 목록, 행, 이벤트 필드).
- **남은 것**: 효과(파티클) 열은 2단계 파티클과 같이. 4단계 가상 카메라가 같은 흔들림 코드를 이어 쓴다.

## v0.4.9 — 1단계 진행 중 (2026-10-18)

### [x] 오디오 (완료 2026-10-18)
- **SDL3 평가 먼저** (플랫폼 계층에서 옮긴 일): 출시 exe와 같은 조건(정적 CRT, /O2, LTO)의 작은 exe로 쟀다. 기준 exe 140 KB.
  SDL3 3.4.18 정적, 영상·렌더·GPU·카메라·센서 끔, 조이스틱·게임패드·햅틱·HIDAPI·오디오만: **2006 KB(+1866 KB)**, HIDAPI까지 꺼도 1728 KB(+1588 KB). 초기화 약 50 ms, 프레임당 폴링 약 3 µs.
  miniaudio 0.11.25 엔진(WASAPI만, MP3·FLAC 끔): **437 KB(+297 KB)**. XInput 직접: +0 KB, 다만 빈 슬롯의 `XInputGetState`가 슬롯당 약 14 µs.
  → SDL3는 exe 예산(5 MB)의 3분의 1을 패드 지원에 쓴다: **쓰지 않는다.** 소리는 miniaudio 자체 백엔드(플랫폼마다 그 플랫폼 백엔드), 게임패드는 플랫폼 계층의 XInput(`docs/DECISIONS.md` 2026-10-18).
- **외부 라이브러리**: `third_party/miniaudio`(0.11.25, MIT-0, `miniaudio.h`), `third_party/stb`(2c980bb: `stb_vorbis.c` v1.22, `stb_truetype.h` v1.26, `stb_image.h` v2.30, MIT). 둘 다 `mealkit.txt` + `gen_notices.ps1`.
  구현은 `audio/MiniaudioImpl.cpp` 하나(경고 끔), 설정은 `audio/MiniaudioConfig.h`(WAV + Ogg Vorbis 디코더, MP3·FLAC·인코딩·생성기 없음, 백엔드는 플랫폼마다 하나). 이름을 `Miniaudio.h`로 했더니 Windows에서 `miniaudio.h`와 같은 파일로 잡혀 이름을 바꿨다.
- **`audio::Engine`** (`mealkit/audio/Audio.h`, 화면 쪽 라이브러리, 헤드리스에는 없다, `GameContext::audio`, `kApiVersion` 11):
  `play(이벤트, 위치, {{"speed", 6}})`·`play2D` → `Handle`, `setParam`, `setPosition`, `stop(handle, 페이드)`, `stopAll`, `setGlobalParam`, `setListener`(엔진이 프레임마다 `FrameParams::cameraPos`·`cameraForward`로), `setBusVolume`, `setMuted`, `update(dt)`.
  장치가 없으면 `init`이 false이고 모든 호출이 아무 일도 하지 않는다(게임은 소리 없이 돈다).
- **소리 이벤트 표** `assets/sounds.table`(엔진 행 타입 `audio::SoundEvent`, 엔진이 등록하는 화면 전용 표 `audio::engineTables()`): 클립(공백으로 나눈 assets 경로), 컨테이너(`random` 직전 클립 반복 안 함 / `switch` 파라미터 값의 정수 / `blend` 파라미터로 이웃한 두 클립 크로스페이드, 최대 4),
  무작위 볼륨·피치(±비율), 파라미터 곡선(파라미터 범위 → 볼륨·피치·로우패스 시작·끝 값, 선형), 3D 여부·감쇠(최소·최대 거리, 선형), 버스, 루프, 스트리밍, 우선순위, 동시 재생 상한과 넘칠 때(`oldest`/`quietest`/`none`), 쿨다운(초).
  0.25초마다 파일 시각을 보고 바뀌면 다시 읽는다(재생 중인 소리는 이름으로 새 행을 찾는다) — **플레이 중에 고치면 바로 들린다.** 에디터 [표] 패널에서 화면 전용 표를 고치면 0.3초에 한 번까지 저장한다.
- **목소리 관리** (엔진이 맡는다): 실제로 섞는 수 상한 = 프로젝트 설정 `audioVoices`(기본 32). 매 프레임 우선순위 × 들리는 크기(볼륨 × 거리 감쇠)로 고르고, 넘치거나 들리지 않는 루프는 **가상 목소리**(믹싱하지 않고 재생 위치만 센다, 다시 들리면 그 위치에서 이어 낸다), 짧은 소리는 끊는다.
  **같은 프레임 묶기**: 시작은 `update()`에서 한 번에 — 같은 이벤트가 여러 번이면 세기(곡선 파라미터, 없으면 볼륨) 큰 순으로 상한만큼. 끊긴 소리는 이유와 함께 64개까지 기록한다.
  효과음은 표를 읽을 때 메모리에 풀어 두고(원본 `ma_sound`를 복사해 재생), 스트리밍 클립은 재생할 때 연다. 엔진은 스테레오로 섞는다(장치가 7.1이어도).
- **버스**: master → music / sfx / ui / voice(표에 다른 이름이 오면 프로젝트 버스로 만든다). 버스마다 레벨 미터 노드(오디오 스레드가 최댓값을 원자 변수에 적는다). 출시 exe는 창이 포커스를 잃으면 음소거(설정 화면에서 끌 수 있게 된다), `--mute`로 끈 채 시작(자동 시험).
- **에디터 [소리] 패널** (`editor/SoundPanel.*`, [보기] > [소리], `--sounds`): 이벤트 미리듣기와 파라미터 슬라이더, 버스 볼륨·레벨, 실제·가상 목소리 목록(들리는 크기·거리), 끊긴 소리와 이유. 에디터가 오디오 엔진 하나를 만들어 플레이와 미리듣기가 같이 쓴다. 플레이를 멈추면 모든 소리를 끈다. 참가 창은 같은 PC라 소리를 끈 채로 연다.
- **물리 충돌 이벤트** (`PhysicsWorld::setContactEvents(minSpeed, maxPerStep)`, `contacts()`): Jolt 접촉 리스너가 새로 닿은 쌍의 접근 속도를 모으고, `step()` 끝에서 세기 → 바디 번호 → 위치로 정렬해 상한만 남긴다 — **물리 스레드 수가 달라도 같다**(단위 테스트 `physics_contact_events`: 1스레드·4스레드 60틱 384개가 같고 상태 해시도 같다). 해시·스냅샷에는 들어가지 않는다.
- **sandbox**: `sim/`이 충돌(`Impact`, 세기 = 다가오던 속도, 틱당 센 것 8개 — 튜닝 `impactSpeed`·`impactsPerTick`)과 점프(`Jump`) 이벤트를 더 남긴다. `GameEvent::radius`는 `value`(밀치기 범위 / 충돌 속도).
  `view/Sounds.cpp`가 틱 이벤트를 소리 이벤트로(다음 항목 "이벤트 → 연출 표"에서 데이터로 옮긴다), `GameSession::start`가 배경음을 튼다. 소리는 `tools/make_sample_sounds.py`가 합성한다(충돌 3, 밀치기, 점프, 점프대, 스폰, 낙사, UI 셋, 16초 배경음 루프. CC0, 시드 고정).
- **측정**: 상자 2000개가 떨어지는 처음 8초 동안 메인 스레드 오디오 시간 프레임당 0.06~0.15 ms, 동시 목소리 최대 7. 출시 exe 1611 KB → 2035 KB(+424 KB). 상태 해시는 그대로(고정 입력 기록 `REPLAY ok`).
- **바꾼 것**: 배경음은 Ogg 인코더가 이 PC에 없어(ffmpeg·oggenc 없음) **WAV 스트리밍**으로 시작했다. Ogg Vorbis 디코딩(stb_vorbis)은 빌드에 들어 있지만 `.ogg` 파일로 아직 시험하지 않았다 — 첫 `.ogg` 에셋이 생기면 확인한다.
  레벨 미터는 버스마다(엔진 미터 노드), 같은 이벤트의 "스위치"는 문자열이 아니라 파라미터의 정수 값으로 고른다.
- **남은 것**: 가림·잔향·더킹·인터랙티브 음악(4단계), 설정 화면의 버스 볼륨(설정 항목), UI 소리(게임 UI 항목). 다른 플랫폼 백엔드는 5단계.

## v0.4.8 — 1단계 진행 중 (2026-10-18)

### [x] 정의 데이터 표 (`.table`) (완료 2026-10-18)
- **왜**: 무기·아이템·적 종류처럼 같은 모양의 행이 여러 개인 데이터를 둘 곳이 없었다(튜닝은 구조체 하나). 4단계 모듈(인벤토리·스킬)·로드아웃·소리 이벤트가 이 위에 놓인다.
- **선언** (`core/Reflect.h`, `kApiVersion` 10): 행 타입은 `MEALKIT_STRUCT`, `MEALKIT_TABLE(PropKind, "tables/props.table", flags)`로 등록한다(`reflect::moduleTables()`, `MEALKIT_OBJECT`와 같은 방식, 게임 DLL은 `GameModule::tables`로 넘긴다).
  `flags`의 `reflect::kTableView`는 화면 전용 표(콘텐츠 해시 밖, 플레이 중 바로 적용 — 쓰는 곳은 오디오·연출 항목).
- **행 참조** `RowRef<Row>` (필드 종류 `Kind::RowRef`): 파일·에디터에는 행 이름 id, 불러온 뒤 행 번호. 해시·비교는 id로, 스냅샷은 id와 번호를 같이. 문자열이 들어 있어 `hasStringField`가 참이다 — 시뮬레이션 상태에는 넣지 않고 번호(u32)를 둔다.
- **파일** (`game/Tables.h`·`.cpp`, 엔진 코어): `row <id>` 다음 줄들이 그 행의 "경로 값"(튜닝과 같은 경로 규칙, 중첩·배열 포함). 행 순서 = 파일 순서 = 행 번호. id는 영문자·숫자·`_ - .`, 표 안에서 겹치면 뒤 행을 버리고 경고.
  - 형식을 아는 쪽: `sim::Table<Row>`(`mealkit/sim/Table.h`: ids + rows, `operator[]`, `get(RowRef)`, `find(id)`)과 `game::loadTable(table)`(등록한 경로).
  - 형식을 모르는 쪽(에디터·해시): `game::TableData`(행마다 `reflect::Instance`), `readTableText`/`writeTableText`, `loadTableData`/`saveTableData`.
  - 참조 풀기: `game::resolveRows(data, {game::tableIndex(table), ...}, &missing)` — 구조체·배열 안의 모든 `RowRef`, 없는 id는 "경로 -> id (행 타입)" 줄. 행 타입은 포인터와 이름으로 찾는다(DLL을 바꿔 끼운 뒤에도).
  - CSV(`exportTableCsv`/`importTableCsv`): 첫 줄 `id` + 필드 경로, RFC 4180(따옴표 안 쉼표·줄바꿈, `""`), Excel의 BOM, 열 순서 무관·모르는 열 무시. 가져오기는 표를 CSV 내용으로 바꾸는 한쪽 방향(저장해야 파일에 쓴다).
  - 콘텐츠 해시: `contentHash(..., tables)`가 시뮬레이션 표의 id와 행 값(`MEALKIT_VIEW_FIELD` 제외)을 넣는다(`game::hashTables`). 출시 exe·헤드리스는 `reflect::moduleTables()`, 에디터는 `GameModule::tables`.
- **에디터 [표] 패널** (`editor/TablesPanel.*`, [보기] > [표], `Ctrl+T`, `--tables`): 표 고르기, 저장·다시 읽기, 행 추가·복제·삭제·위아래, id 칸(Enter·다른 곳을 누르면 적용, 규칙 검사), 칸마다 필드 위젯(머리글은 필드 설명),
  CSV 가져오기·내보내기(파일 대화상자). 아래에 **없는 행 참조** 목록: 표끼리, 그리고 에디터가 0.5초마다 튜닝과 씬 물체 필드도 검사해 넣는다. 인스펙터·튜닝의 `RowRef` 필드는 드롭다운(없는 id는 빨간색 `(?)`).
  플레이를 누르면 고친 표를 저장한다(게임은 매치를 시작할 때 파일을 읽는다). 표 값은 DLL 타입이라 DLL을 내릴 때 텍스트로 보존했다가 새 타입으로 다시 만든다.
- **sandbox**: `sim/Tables.h`의 `PropKind`(크기·늘림 범위, 밀도, 최소 질량, 마찰, 반발) + `assets/tables/props.table`(`crate`, `heavy`, `bouncy`). 튜닝의 상자 값 8개는 표로 옮겼다.
  `PropSpawnArea.kind`(`RowRef<PropKind>`, 기본 `crate`)가 종류를 고르고, `World::loadScene`이 `propKinds.find(id)`로 번호를 정한다(없으면 첫 행과 경고). `Headless::startWorld`가 매치마다 `game::loadTable`로 읽어 `World::reset`에 넘긴다.
  `crate`가 예전 튜닝 값과 비트까지 같아 **상태 해시가 그대로**다(고정 입력 기록 901틱 `REPLAY ok`). 콘텐츠 해시가 바뀌어 기록을 다시 만들었다.
- **검증**: 단위 테스트 `tables`(파일 왕복, 겹친·잘못된 id, CSV 왕복과 따옴표·줄바꿈·BOM·열 순서, 참조 풀기와 없는 id, 해시는 id로, 스냅샷), 에디터 캡처에서 [표] 패널과 인스펙터 드롭다운.
- **남은 것**: 화면 전용 표를 플레이 중 다시 읽는 `LiveTable`은 첫 화면 전용 표(소리 이벤트, 오디오 항목)와 같이. 표 사이의 행 참조를 에디터에서 따라가기(누르면 그 행으로)는 나중에.

## v0.4.7 — 1단계 진행 중 (2026-10-18)

### [x] 예제(sandbox) 데이터 분리 정리 (완료 2026-10-18)
- **왜**: 데이터 장치(튜닝·씬 필드·화면용 튜닝)는 갖췄지만 예제 코드에 숫자가 남아 있었다. 새 프로젝트는 이 코드를 복사해 시작한다.
- **`Tuning`으로** (`sim/Tuning.h`, `assets/game.tuning`): 캐릭터 반 높이·반지름·질량, 발밑 감지 여유, 점프 가능 상승 속도, 스폰 줄 간격 / AI 목표 거리 범위, 점프·밀치기 확률(1/N 정수, 난수열이 같도록) /
  상자 크기·늘림 범위, 마찰, 반발, 최소 질량 / 씬 충돌 물체 마찰 / 점프대 감지 높이. 맵과 무관한 게임 규칙 값이다.
- **씬 물체로** (`sim/Objects.h`): 맵마다 다른 값은 에디터에서 눈으로 놓는 **영역 물체**로 옮겼다.
  - `MatchArea`(경기 영역, 씬에 하나): 상자 자체가 규칙이다 — 가로·세로 = AI가 다니는 범위·떨어진 상자를 다시 떨어뜨리는 범위, 바닥 = 낙사 높이(-30), 위 = 다시 떨어뜨리는 높이(30). 필드가 없다.
    없는 씬(엔진 기본 아레나)은 경고 후 예전 값(±40, -30..30)으로 돈다(`// const-ok`).
  - `PropSpawnArea`(상자 생성 영역, 여러 개 가능): 영역 안에 `count`개를 흩어 놓는다(씬 순서, 같은 난수열). 상자 Pool 용량 = 합계. `matchData`(`--bodies`·`--match-data`)는 이제 "0 = 씬, 아니면 그 수로"(영역 비율대로 나누고 나머지는 마지막 영역, 정수 계산).
  - 기본 스폰 지점 표(`kDefaultSpawns`)는 지웠다: 스폰 지점이 없는 씬은 원점에서 `spawnSpacing` 간격으로 줄 선다.
  - `arena.scene`에 `Match Area`(0,0,0 / 80×60×80)와 `Box Spawn Area`(0,13,0 / 80×24×80, 2000개)를 더했다 — 예전 상수와 경계가 비트까지 같다.
- **엔진: 영역 물체 종류** `reflect::kObjectArea`(`kObjectFlags`): 크기만 쓰는 보이지 않는 상자. 에디터는 모서리 12개만 그리고(두께는 카메라 거리 비율, 안이 가려지지 않는다) 모서리를 눌러야 고른다(안을 누르면 안의 물체).
  [추가] 메뉴로 만들면 10×4×10. 게임은 `scene::visibleInGame()`(표시·영역은 그리지 않음)과 `scene::areaBounds()`(축 정렬 상자)로 쓴다.
- **화면 쪽**: 충격파 발밑 높이는 `world.tuning.pawnHalfHeight`를 인자로 받는다. 로그 "World ready"·"Play mode"는 상자 수 대신 인원·움직이는 바디를 보인다(check.ps1의 정규식도).
- **SimCheck 경고 SIM012** (ROADMAP의 "도입 때 정한다"를 정함): `sim/`의 실수 리터럴 중 0·1·2·0.5가 아닌 것을 **경고**한다(빌드는 계속). `MEALKIT_*( ... )` 필드 선언 안(기본값은 데이터 자체)과 `// const-ok: <이유>` 줄은 뺀다.
  sandbox에서 남은 것은 셋이었다: 입력 양자화 127, 부피 공식의 8, 0으로 나누지 않는 문턱 1e-4 — 모두 `const-ok`. `kTickRate`도 틱레이트 항목까지 `const-ok`.
- **결정론**: 계산 순서까지 그대로 옮겨 **상태 해시가 한 비트도 바뀌지 않았다** — 저장소의 고정 입력 기록 901틱이 그대로 `REPLAY ok`(5114b2f8798f3f1d). 콘텐츠 해시는 바뀌어 기록을 다시 만들었다.
- **템플릿** (엔진 버전 0.4.7): 예제 코드가 템플릿이라 버전을 올린다. 템플릿 CLAUDE.md에 "숫자는 어디에 두나" 규칙과 SIM012를 적었다.
- **남은 것**: 행이 여러 개인 데이터(무기·소리 이벤트)는 다음 항목 `.table`.

## v0.4.6 — 1단계 진행 중 (2026-10-18)

### [x] 프로젝트 폴더 구조 강제 (완료 2026-10-18)
- **왜**: 원본 파일이 출시 폴더에 섞이고, 외부 라이브러리 정보는 세 곳(`VERSIONS.txt`, `THIRD_PARTY_NOTICES.txt`, 코드)을 손으로 맞춰 왔다. 폴더 구조를 고민하는 시간을 없앤다.
- **규칙 한 곳** `tools\check_layout.ps1 [-Project <폴더>]`: check.ps1("Project layout + notices"), Export(`game_export`가 맨 먼저 부르고 오류면 실패), 에디터(프로젝트를 열 때 `ChildProcess`로 돌려 오류는 알림 창, 경고는 상태 표시줄·로그)가 모두 이 스크립트를 쓴다. C++에 규칙을 따로 두지 않았다.
  - `assets/` 바로 밑: 종류 폴더(`scenes models textures sounds effects ui fonts tables strings lut`)와 정해진 파일(`game.tuning view.tuning input.bindings feedback.table sounds.table`)만.
  - 종류 폴더 안: 그 종류의 확장자만(하위 폴더는 자유). `fonts/`는 글꼴 라이선스 `.txt`도, `ui/`·`effects/`는 `.png`도 받는다.
  - 외부 도구 원본(`.blend .psd .kra .penpot .fbx .aup3 ...`)은 `assets/` 어디에도 두지 않는다(→ `art/`). `.autosave`·`.gitkeep`은 본다.
  - `third_party/<lib>/`: `mealkit.txt`(version·license·source 필수)와 라이선스 파일, 허용 라이선스(MIT, MIT-0, BSD-2/3, Zlib, Unlicense, PublicDomain, CC0-1.0, OFL-1.1, BSL-1.0, ISC), 게임 프로젝트는 엔진에 이미 있는 라이브러리 이름 금지.
  - 정해진 폴더(`source assets art modules third_party`, 종류 폴더)가 없으면 경고.
- **외부 라이브러리 메타데이터**: 엔진 `third_party/<lib>/mealkit.txt`(name, version, license, licenseFile, source, usedIn, inGame, note, kept, why) 6개를 썼다.
  `tools\gen_notices.ps1`이 `third_party/VERSIONS.txt`(표 + 이유 + SDK 행)와 `THIRD_PARTY_NOTICES.txt`를 만든다(라이브러리 이름순, 원본 LICENSE 그대로). `-Check`는 check.ps1이 부른다(손으로 고치면 실패).
  `-Project <폴더> -Out <파일>`은 출시 폴더용: MealKit 자신의 MIT 고지 + 게임 exe에 들어가는 엔진 라이브러리(`inGame yes`: Jolt, cgltf, meshoptimizer) + 프로젝트 `third_party/` + `assets/fonts/*.txt`.
  Steamworks처럼 저장소에 넣지 않는 SDK는 스크립트의 SDK 목록에서 표에만 들어간다.
- **게임 라이브러리** (`mealkit_add_game`): 프로젝트 `third_party/*/mealkit.txt`를 찾아, `CMakeLists.txt`가 있으면 `add_subdirectory` + `cmakeTarget` 링크, 없으면 헤더 전용(`include` 키, 기본 그 폴더)으로 게임 exe·DLL에만 붙인다(헤드리스 타깃에는 붙이지 않는다).
  SimCheck가 `--third-party <폴더>`를 받아, 그 폴더에서 찾아지는 헤더나 경로에 `third_party/`가 있는 include를 `sim/`에서 **SIM011**로 막는다.
- **Export**: 레이아웃 검사 → 고지문 합치기 → `MealKitExport.cmake`(exe가 5 MB를 넘으면 실패, 크기를 출력, `.gitkeep`은 복사하지 않음).
- **템플릿** (엔진 버전 0.4.6): `templates/project`에 `art/ modules/ third_party/`와 `assets/<종류>/`의 `.gitkeep`. 새 프로젝트는 모든 폴더를 갖고, `upgrade_project`는 없는 `.gitkeep`을 "added"로 만든다(파일은 옮기지 않는다 — 검사가 위치를 알려 준다).
  템플릿·엔진 `.gitignore`의 `*.png`를 캡처 파일(`/screenshot*.png /capture*.png /out*.png`)로 좁혔다(텍스처·`icon.png`가 올라간다). README 폴더 표를 새 구조로.
- **정한 것**: 소리 이벤트 표는 `assets/audio/sounds.table`이 아니라 정해진 단일 파일 `assets/sounds.table`(`feedback.table`과 같은 자리, `assets/` 밑에 `audio/` 종류 폴더가 없다). Penpot 원본 백업은 `design/`이 아니라 `art/ui/`(CODING_MODEL의 `art/` 답과 맞춘다).
- **검증**: 엔진 저장소 `check_layout` OK(0 경고), 잘못 놓은 파일 셋(`assets/foo.txt`, `models/x.blend`, `scenes/y.png`)이 오류 3개. 새 프로젝트(`new_project`)가 0 경고로 통과.
  새 프로젝트에 헤더 전용 라이브러리를 넣어 `view/`에서 쓰고 `build.bat Export`: 레이아웃 OK → 고지문 5개(MealKit, cgltf, Jolt, meshoptimizer, 그 라이브러리) → exe 1611 KB → Export OK. 같은 헤더를 `sim/` 파일에서 include하면 SIM011.
- **남은 것**: `patches/*.patch` 적용 도구는 고칠 원본이 생길 때. 에디터 에셋 브라우저(4단계)가 가져오기를 종류 폴더로 보내는 것.

## v0.4.5 — 1단계 진행 중 (2026-10-18)

### [x] 화면용 튜닝 (`ViewTuning`) + 시간 값은 초로 (완료 2026-10-18)
- **왜**: 튜닝(`Tuning`)은 시뮬레이션 값이라 콘텐츠 해시에 들어가고 여러 명 플레이 중에는 바꿀 수 없다. 카메라·색·효과처럼 보이기만 하는 값은 둘 곳이 없어 `view/` 코드 상수로 남아 있었다.
  시간 값을 틱으로 적으면 틱레이트(1단계 마지막 항목)를 바꿀 때 데이터를 모두 고쳐야 한다.
- **필드 타입 둘** (`core/Reflect.h`, `kApiVersion` 9):
  - `mealkit::Ticks { u32 value; }`: 메모리·해시·`sim/`은 정수 틱, 파일·에디터는 초. 파일을 읽을 때 `Ticks::seconds()`가 프로세스 틱레이트로 한 번 반올림한다(배정밀도 곱셈 + `lround`, 음수는 0).
    기본값도 초로 쓴다: `MEALKIT_FIELD(Ticks, respawn, Ticks::seconds(1.5f), ...)`. `u32`로 저절로 바뀌어 `TickTimer::start(tick, world.tuning.respawn)`처럼 그대로 쓴다. 텍스트는 `%.6g`초(다시 읽으면 같은 틱).
  - `mealkit::Color { u32 value; }`: rgba8 sRGB(`render::rgba`와 같은 배치), 파일에는 `#RRGGBB`/`#RRGGBBAA`. 에디터는 색 고르기 위젯.
  - 틱레이트: `reflect::setTickRate()`/`tickRate()`(기본 30). `setProjectRoot()`처럼 프로젝트를 열 때 한 번 정하는 프로세스 설정이다(CODING_MODEL "엔진 기능에 어떻게 접근하나" 예외에 더함). 지금은 아무도 바꾸지 않고, "틱레이트를 프로젝트 설정으로" 항목에서 프로젝트 설정이 정한다.
- **화면용 튜닝**: 게임이 `using ViewTuningType = ViewTuning;`(GameSession)을 선언하면 `MEALKIT_GAME_MODULE`이 `GameModule::viewTuningType`으로 넘긴다(없어도 된다).
  - 값은 `assets/view.tuning`. 엔진 `game::LiveTuning`(`game/GameData.h`)이 읽고 0.25초마다 파일 시각을 봐서 바뀌면 다시 읽는다(지운 줄은 기본값). 파일이 아직 없으면 `game.tuning` 옆에 생길 때 읽는다.
  - 게임은 `ctx.viewTuning<ViewTuning>()`(`GameContext::viewTuningData`)로 매 프레임 읽고 보관하지 않는다. 엔진이 넘기지 않으면 선언의 기본값.
  - 출시 exe(sandbox `main.cpp`)·에디터·참가 창이 하나씩 갖는다. 에디터 [튜닝] 패널은 탭 두 개("게임", "화면")이고, 화면 탭 값은 고치는 즉시 에디터 안 게임에 보인다.
    여러 명 플레이에서는 에디터가 `build\Editor\play\play.view.tuning` 사본을 0.2초에 한 번까지 다시 쓰고, 참가 창(`--view-tuning`)이 그 파일을 다시 읽는다 — 그래서 **여러 명 플레이 중에도 바로 바뀐다**. 게임 탭은 지금처럼 혼자 플레이에서만 바로 적용.
  - 에디터 튜닝 코드를 파일 하나 단위(`TuningDoc`: 값, DLL을 바꿔 끼울 때 보존할 텍스트, 저장 여부)로 묶어 두 탭이 같이 쓴다.
- **sandbox**: `view/ViewTuning.h`에 카메라(시야각·감도·줌·거리·높이 각 범위·시작 값·자르기 면), 캐릭터 겉모습(몸·얼굴 크기와 색, AI 색, 내 캐릭터 표시), 충격파(시간·조각 수·두께·색·고리 크기), 플레이어 색 8개, 상자 색 6개를 옮겼다.
  `Camera`·`Draw`·`Effects`가 값을 인자로 받는다(코드 상수 없음). 스폰·점프대 이벤트의 효과 크기(1.2, 2.0)는 시뮬레이션에서 빼고 화면용 튜닝으로.
  `Tuning`의 `pushCooldownTicks`·`respawnTicks`·`aiThinkTicks`는 `pushCooldown`·`respawn`·`aiThink`(`Ticks`, 0.5·1.5·1.33333초), 점프대 `cooldownTicks`는 `cooldown`(0.666667초). `game.tuning`·`arena.scene`의 키도 바꿨다.
- **결정론**: 틱 값이 같아(15·45·40·20틱) 상태 해시가 그대로다 — 저장소의 고정 입력 기록이 바꾸기 전 해시와 901틱 모두 같았다(`REPLAY ok`). 씬 필드 텍스트가 바뀌어 콘텐츠 해시는 달라졌으므로 기록을 다시 만들었다(해시 줄은 같다).
  화면용 튜닝은 `sim/`에서 읽을 수 없다(`view/` 헤더는 SimCheck SIM009).
- **검증**: Release·Editor 빌드, 헤드리스 `mp_test` 1+3 PASS, 에디터 혼자 플레이 캡처에서 [튜닝] 패널 탭과 `Ticks` 위젯(0.500 s, 툴팁에 틱 수).
- **남은 것**: 틱레이트를 실제로 바꾸는 것은 마지막 항목. `.table`의 시간 열도 `Ticks`를 쓴다(다음 항목들).

## v0.4.4 — 1단계 진행 중 (2026-10-18)

### [x] 저장소 공개 범위 (결정 2026-10-18)
- 공개 시점은 날짜·단계로 정하지 않고 사용자가 요청할 때. 외부 기여는 DCO(`git commit -s`, CLA 없음), 기준 샘플 에셋은 CC0. 이유는 `docs/DECISIONS.md` 2026-10-18.
- 공개할 때 할 일: `CONTRIBUTING.md`(DCO), 에셋 폴더의 CC0 고지, Windows on ARM CI 러너, 문서 Read the Docs.

### [x] 문서 저장소 (완료 2026-10-18)
- **왜**: 사용 설명이 `docs/GUIDE.md` 한 파일(한국어)에 쌓이고 있었다. 엔진 코드와 따로 고칠 설명을 나누고, 영어·한국어와 엔진 버전별로 보여 준다.
- **저장소** `D:\MealKit-docs`(git, 아직 원격 없음): Sphinx 8.2.3 + MyST 4.0.1 + sphinx-rtd-theme, 버전은 `requirements.txt`에 고정. 목차는 ROADMAP대로
  소개(소개·요구사항·라이선스) / 시작하기(엔진 빌드·첫 프로젝트·작업 흐름·에디터 빠른 시작) / 매뉴얼(엔진 폴더와 프로젝트·에디터·에셋·멀티플레이와 결정론·빌드와 출시·성능) / 쿡북 / 엔진 개발(소스 구조·테스트·기여) / 레퍼런스(명령줄) / 용어집.
  GUIDE.md의 모든 절이 자리를 받았다(현재 상태·측정값·보이스 챗 → 소개, 빌드 → 시작하기, 게임 실행·헤드리스·Steamworks → 멀티플레이, 테스트·CI·구조·프레임 흐름 → 엔진 개발).
  명령줄 레퍼런스는 게임·헤드리스·에디터의 `--help`와 `tools\` 스크립트를 표로 새로 썼다.
- **번역**: 영어 원문, 한국어는 `locale/ko/LC_MESSAGES/<문서>.po`(문서마다 하나, 줄 위치 없음 → diff가 작다). 538문장. 처음 번역은 GUIDE.md 한국어 문장을 영어 원문과 같은 구조로 나눈 사본에서 순서대로 짝지어 만들었다.
  코드 블록 안의 주석도 번역한다: Sphinx 자체의 코드 블록 번역은 번역문을 reST(`::`)로 다시 읽어 MyST에서는 내용이 사라지므로, `conf.py`의 작은 변환(`LiteralBlockTranslator`)이 카탈로그에서 바로 바꾼다(gettext 빌더만 코드 블록을 뽑는다).
- **검사** `tools/build.py`(로컬·CI 공통): `html`(영어 `_build/html/en`, 한국어 `_build/html/ko`, 경고 = 오류), `update`(sphinx-intl: 새 문장은 빈 칸, 바뀐 문장은 fuzzy), `check`(두 언어 빌드 + 미번역·fuzzy·원문 없는 번역 0 + `ENGINE_VERSION`과 엔진 `VERSION` MINOR 비교), `linkcheck`.
  영어 문장 하나를 바꾸면 `check`가 실패하고 `update` 뒤 그 문장이 fuzzy가 되는 것을 확인했다. CI는 `.github/workflows/docs.yml`(ubuntu, 엔진 저장소가 비공개인 동안은 `ENGINE_REPO_TOKEN` 비밀 값이 있을 때만 버전을 비교).
  엔진 저장소 파일 링크는 `{engine}` 역할(GitHub `master`)이고, 비공개인 동안 링크 검사에서 뺀다.
- **엔진 쪽 정본**: GUIDE "결정론 규칙"을 `docs/CODING_MODEL.md` "결정론 규칙 (시뮬레이션 코드)" 절로, 외부 라이브러리 표의 "이유" 열을 `third_party/VERSIONS.txt`로 옮겼다. 문서 저장소는 이것들을 복사하지 않고 링크한다.
- **참조 바꾸기**: CLAUDE.md, ROADMAP, DECISIONS 보류 표, SimCheck 주석·메시지(→ CODING_MODEL), `.gitignore`·CMake·`check.ps1`·`record_replay.ps1`·CI 주석, README, 템플릿 README. `docs/GUIDE.md`는 지웠다(지난 결정·변경 기록의 언급은 기록이라 그대로 둔다).
- **버전 붙은 문서 주소**: CMake가 `VERSION`에서 `MEALKIT_DOCS_URL`(`https://mealkit.readthedocs.io/en/<MAJOR.MINOR>/`)을 만들어 넘기고(`core/Version.h`의 `kDocsUrl`), "없음" 온라인 백엔드와 sandbox의 Steam 오류 메시지가 `manual/multiplayer.html#steamworks`를 가리킨다.
  Read the Docs 프로젝트 이름 `mealkit`은 가정이다 — 공개할 때 다르면 CMake 한 줄을 고친다. 그때까지 이 주소는 열리지 않는다.
- **에디터 `--lang ko|en`**: 이번 실행만 에디터 언어를 바꾼다(사용자 설정은 그대로). 문서 스크린샷을 언어별로 `--capture`하기 위한 것.
- **템플릿**: 템플릿 README의 결정론 규칙 링크와 sandbox 메시지가 바뀌어 엔진 버전 0.4.3 → 0.4.4.
- **ROADMAP 방식과 다른 점·남은 것**: 문서 브랜치는 `master`만 만들었다(0.5부터 MINOR 브랜치). 스크린샷은 아직 없다(옵션만 넣음). `--dump-reference`로 레퍼런스 생성, Weblate는 나중에.
  영어 문서의 CODING_MODEL 절 이름은 영어로 옮겨 적었다(CODING_MODEL 자체는 한국어). 기존 GUIDE 내용 다시 쓰기는 페이지마다 조금씩.

## v0.4.3 — 1단계 진행 중 (2026-10-16)

### [x] CI 빌드 매트릭스 (완료 2026-10-18)
- **왜**: 플랫폼·컴파일러가 늘면 사람이 모든 조합을 빌드해 해시를 비교할 수 없다. 5단계 크로스플레이의 바탕(OS·CPU·컴파일러 사이 같은 틱 해시)을 첫날부터 매 커밋 확인한다.
- **워크플로** (`.github/workflows/ci.yml`, 푸시·PR·수동): Windows(windows-2022) 3개 — `windows-msvc`(Release, 기준), `windows-clang-cl`(`build.bat ClangCL`), `windows-msvc-editor`(`build.bat Editor`, 단위 테스트만) — 는 로컬과 같은 `build.bat`을 그대로 부른다.
  헤드리스 2개 — `linux-x64-clang`(ubuntu-24.04, apt clang + ninja), `macos-arm64-clang`(macos-15 Apple Silicon, Apple clang) — 는 `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++` 후 `sandbox_headless`·`mealkit_tests`만 빌드한다(`MEALKIT_HEADLESS_ONLY`, SimCheck도 빌드 때 돈다).
  `windows-msvc`는 `check_strings`·`check_platform`도 돈다. 빌드마다 결과 폴더를 아티팩트로 올리고, 마지막 `compare` 작업이 모아 비교한다. 같은 브랜치의 새 커밋은 이전 실행을 취소한다.
- **빌드마다** `tools/ci_test.ps1`(Windows PowerShell 5.1·pwsh 7 둘 다): 단위 테스트와 `detmath-bits` 줄, 저장소의 고정 입력 기록(`samples/*/replays/*.rec`)을 `<샘플>_headless --replay-inputs --hash-log`로 재생(`REPLAY ok` = 기록에 든 MSVC 해시와 틱마다 같음),
  헤드리스 `mp_test`(1+3, 900틱, 클라이언트마다 물리 스레드 수 다름). mp_test가 실패하면 피어별 출력도 아티팩트에 넣는다.
- **비교** `tools/ci_compare.ps1`: 모든 빌드의 `detmath-bits`와 틱별 해시 로그를 `windows-msvc`와 비교해 Markdown 표를 작업 요약(`$GITHUB_STEP_SUMMARY`)에 쓰고, 다르면 처음 갈라진 줄(틱)을 보여 주며 실패한다.
- **고정 입력 기록** `samples/sandbox/replays/arena_4p_seed7.rec`(36 KB): 헤드리스 호스트 1 + 클라이언트 3, seed 7, 상자 2000개, 901틱(0~900)의 확정 입력과 해시. 혼자보다 네 명이 서로 밀치는 쪽이 물리 접촉을 더 많이 거친다.
  `tools/record_replay.ps1`이 만든다(다시 만들면 바이트까지 같은 파일이 나온다). 시뮬레이션 결과를 일부러 바꾸면 이것으로 다시 만들어 같이 커밋한다 — 로컬 `check.ps1`에 "Committed replays" 단계를 더해 푸시 전에 알 수 있게 했다.
  `.gitattributes`에서 `.rec`(와 `.glb`·`.png`·`.ico`)를 바이너리로 두어 Windows 러너의 `core.autocrlf`가 바꾸지 않게 했다.
- **로컬 검증**: Release·ClangCL·Editor 빌드에서 `ci_test.ps1`가 모두 PASS(`detmath-bits f67a477cb2496808`, 기록 재생 901틱 같음), `ci_compare.ps1`가 PASS, 해시 한 줄을 일부러 바꾸면 FAIL. `check.ps1 -Quick` 9개 통과.
- **CI 결과** (실행 #4, 커밋 6b548bc, 약 4분): 5개 빌드 모두 통과. `detmath-bits f67a477cb2496808`이 모든 빌드에서 같고, 고정 기록 901틱의 해시가
  **Windows MSVC·clang-cl(x64), Linux x64 Clang(apt clang), macOS ARM64 Apple clang에서 모두 같다** — OS·CPU 아키텍처·컴파일러 사이 결정론을 처음 확인했다. 각 러너 안의 헤드리스 mp_test도 PASS.
- **첫 실행들에서 고친 것**:
  `.gitignore`의 `build/`가 Windows에서 대소문자를 가리지 않아 `third_party/JoltPhysics/Build/CMakeLists.txt`와 `engine/res/MealKitEditor.png`(에디터 리소스)가 저장소에 없었다 → 출력 폴더를 `/build/` 등 최상위로 고정.
  `Reflect.cpp`의 MSVC 전용 `sscanf_s` → `strtoul`. Jolt 자체 LTO가 Clang에서 LLVM 비트코드 라이브러리를 만들어 Linux GNU ld가 못 읽었다 → 비MSVC에서는 Jolt LTO 끔(엔진과 같게).
  헤드리스 네트워크 루프가 `FrameLimiter`(1000fps, 남은 시간 1ms 이하라 자지 않음)로 바쁜 대기를 해, vCPU 4개 Windows 러너에서 피어 4개가 물리 작업 스레드를 굶겨 60초 안에 끝나지 않았다
  → 틱이 나아가지 않은 반복에서만 1ms 자도록(`core/Clock.h`의 새 `sleepSeconds`, Windows 고해상도 대기 타이머 / POSIX nanosleep). 로컬 1+3 900틱은 그대로 약 5초.
  `ci_compare.ps1`은 빌드 결과가 빠지면 실패하도록(`-Expect`), `ci_test.ps1`은 mp_test 실패 때 피어 로그를 출력하도록, 헤드리스 빌드는 `ninja -k 0`으로 오류를 한 번에 보이도록 했다.
  `platform/posix/`는 고칠 것 없이 Linux·macOS에서 컴파일·동작했다.
- **ROADMAP 방식과 다른 점·남은 것**: 액션(checkout·upload/download-artifact v4)이 Node.js 20 경고를 낸다(버전 올리기는 따로). Windows on ARM은 넣지 않았다(ARM Windows 러너는 공개 저장소용이라 저장소 공개 범위 결정 뒤). ROADMAP의 `check_layout`은 아직 없는 검사라 빠졌고(배치 검사는 디자이너 항목), SimCheck는 각 빌드 안에서 돈다.
  기록은 sandbox 하나뿐이다 — 샘플이 늘면(기준 샘플 규칙 항목) `samples/<샘플>/replays/`에 두면 `ci_test.ps1`이 자동으로 재생한다. 3단계 디싱크 진단 도구가 생기면 비교 실패 때 그것을 부른다.

### [x] 헤드리스 시뮬레이션 (완료 2026-10-18)
- **왜**: CI가 GPU·창 없는 러너에서 컴파일러·OS·CPU 사이 틱별 해시를 매 커밋 비교하려면(5단계 크로스플레이의 바탕) 시뮬레이션만 도는 실행 파일이 먼저 있어야 한다.
- **엔진 라이브러리 나누기** (`engine/CMakeLists.txt`): 코어(`mealkit_core`: core, game, sim, physics, scene, asset, net, `render/Mesh`, 플랫폼의 시간·파일·소켓·시스템)와 화면 쪽(창·입력·렌더러·온라인 서비스·에디터용 플랫폼 기능).
  Game 빌드는 `mealkit_core` + `mealkit`(코어를 PUBLIC으로 링크) 두 정적 라이브러리, Editor 빌드는 예전처럼 둘을 합친 `mealkit.dll`. 단위 테스트는 코어만 링크한다.
  `ModelCache`는 렌더러를 직접 부르지 않고 업로드 함수 포인터를 갖는다(`setRenderer`는 헤더 인라인이라, 부르지 않는 헤드리스는 렌더러 없이 링크된다). 에셋 폴더 경로 구분자를 `/`로.
- **비Windows CMake** (`cmake/MealKit.cmake`): Windows는 그대로 MSVC·clang-cl. Linux·macOS의 Clang·GCC는 `MEALKIT_HEADLESS_ONLY`로 코어 + 헤드리스 타깃 + 단위 테스트 + SimCheck만 만든다.
  컴파일 옵션 `-Wall -Wextra -fno-rtti -ffp-contract=off -fno-fast-math`, 스레드 라이브러리. Jolt는 `CROSS_PLATFORM_DETERMINISTIC`로 스스로 `-ffp-contract=off`를 붙인다. LTO는 MSVC만.
  **이 PC에 Linux·macOS 도구(WSL 배포판 포함)가 없어 비Windows 구성은 한 번도 돌려 보지 않았다** — CI 빌드 매트릭스 항목에서 처음 빌드하며 고친다(POSIX 플랫폼 구현도 마찬가지).
- **게임 쪽 경계** (`docs/CODING_MODEL.md` "창 없이 게임을 돌리는 방법", 사용자와 정함): 게임은 `app/Headless.cpp`에 클래스 하나와 `MEALKIT_HEADLESS_GAME(클래스)`.
  클래스는 `World`·`TuningType`·`ProjectSettingsType` 타입, `kTickDt`, `startWorld(World&, MatchInfo, 튜닝, 평평한 씬, 모델, 물리 스레드)`, `botInput(틱, 플레이어)`, 선택 `defaults(옵션, 게임 프로젝트 설정)`을 내놓는다.
  엔진(`mealkit/game/Headless.h/.cpp`)이 옵션 해석, 프로젝트·씬·튜닝 읽기, 콘텐츠 해시(출시 exe와 같은 값이라 창 있는 피어와도 접속된다), 세션(혼자·방장·참가), `game::Simulation` 실행, 출력(`FINAL`, `SHADOW`, `REPLAY`)을 맡는다.
  `mealkit_add_game()`은 Game 빌드에서 `<이름>_headless`(sim/ + `Headless.cpp`, `MEALKIT_HEADLESS_TARGET`이면 매크로가 `main`도 만든다)를 만든다(`HEADLESS` 인자, 파일이 없으면 만들지 않는다).
  출시 exe는 `--headless`가 있으면 `mealkitHeadlessMain`을 부른다. sandbox `GameSession`도 매치 시작과 봇 입력에 같은 함수를 쓴다(`botInput`은 `view/LocalInput`에서 옮겼다. CRT 삼각함수를 써서 `sim/`에는 둘 수 없다).
- **실행 방식**: 입력은 항상 봇. 혼자면 쉬지 않고 돌고(900틱 약 2초, 그림자 시뮬레이션 포함), 네트워크면 한 번에 최대 8틱씩 따라잡고 대기 중에는 1ms씩 잔다. 호스트는 `--auto-start`(기본 `--players`) 인원이 모이면 시작.
  창 옵션(`--bot`, `--vsync`, `--fps`, `--size`, `--debug`)은 받아서 버린다(mp_test가 같은 인자를 넘긴다). 매치 값 옵션 이름은 엔진 쪽 `--match-data`(sandbox 창 모드는 `--bodies`와 같다).
- **해시 로그·입력 기록** (`mealkit/game/InputRecord.h/.cpp`): `PlayOptions::hashLogPath`(틱마다 `<틱> <해시16진>`)와 `recordInputsPath`(확정된 틱 입력 + 해시, 리틀 엔디언, 헤더에 시드·인원·매치 데이터·콘텐츠 해시)를 `game::Simulation`이 쓴다.
  그래서 창 있는 게임(`--hash-log`, `--record-inputs`)과 헤드리스가 같은 파일을 만든다. `--replay-inputs`는 네트워크 없이 `World`를 처음부터 다시 돌리고 틱마다 기록된 해시와 비교한다(`REPLAY ok` / `REPLAY DESYNC tick=N`, 콘텐츠 해시가 다르면 경고).
  `PlayOptions`가 바뀌어 `kApiVersion` 8. `LockstepSession::contentHash()` 추가.
- **도구**: `mp_test.ps1 -Headless`(모든 피어에 `--headless`, 1+3 900틱 약 5초, 창 모드 약 30초). `check.ps1`에 "Headless (mp_test, record + replay)"(헤드리스 mp_test + `sandbox_headless`로 기록 → `sandbox.exe --headless`로 재생, 해시 로그 비교)와
  "Headless = window"(창 있는 게임의 그림자·되감기 실행의 해시 로그가 헤드리스와 같은지)를 더했다. 단위 테스트 `input_record`(기록 왕복).
- **검증**: Release·Editor 빌드, `check.ps1` 19개 통과. seed 7 900틱에서 헤드리스·재생·창 있는 게임의 해시 로그 901줄이 모두 같다. 실행 파일 크기: `sandbox.exe` 1.6 MB, `sandbox_headless.exe` 1.5 MB.
  컴파일러 비교: `build.bat ClangCL`의 `sandbox_headless.exe`로 MSVC 빌드가 만든 기록 파일을 재생해 `REPLAY ok`, 해시 로그가 MSVC와 같다.
  첫 `check.ps1`에서 "Editor play (3 players)"가 한 번 실패했다(참가 창들이 400프레임 안에 접속하지 않음). 같은 시험을 따로 세 번, `check.ps1` 전체를 한 번 더 돌려 모두 통과했고 원인은 찾지 못했다(이번 변경과 관련된 증거는 없음). 다시 나오면 참가 창 로그(`build\Editor\play\player*.log`)부터 본다.
- **남은 것**: Linux·macOS 실제 빌드와 CI(다음 항목), 플레이하지 않는 전용 중계 프로세스(락스텝 세션의 방장이 지금은 플레이어 0이라 세션 쪽 변경이 필요하다. 3단계 "호스트 이전"과 같이 본다).

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
