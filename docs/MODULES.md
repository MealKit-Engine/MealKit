# 소스 모듈 시스템 기획

4단계 "소스 모듈 시스템"의 구현 기획이다. 방향은 `docs/DECISIONS.md` 2026-10-16·2026-10-21에서 이미 정했다: 소스째 복사, 자기 상태만 받는 함수, 업그레이드 도구 방식 갱신.
이 문서는 그 위에서 **무엇을 만들고, 어떻게 동작하고, 모듈을 어디에 두고 어떻게 찾을지**를 정한다. 결정은 `docs/DECISIONS.md` 2026-10-28 "소스 모듈". 항목이 끝나면 규칙은 `docs/CODING_MODEL.md`와 문서 저장소 "Writing modules"로, 구현은 `docs/CHANGELOG.md`로 옮기고 이 문서는 지운다.

초안 1과 다른 점(2026-10-28 사용자 의견): 공식 모듈을 엔진 저장소에 두지 않는다. **공식 모듈도 다른 사람 모듈과 똑같이 자기 저장소에서 만들고**, 모든 모듈은 **모듈 목록 저장소(레지스트리) 하나**에서 모아 본다.

## 한 줄 요약

모듈은 **프로젝트 `modules/<이름>/`에 복사된 소스 폴더**다. 빌드는 그 폴더의 `sim/`·`view/`를 게임 코드처럼 모아 SimCheck와 시뮬레이션 버전 해시에 넣는다. 에셋은 `modules/<이름>/...` 경로로 읽는다.
게임 코드가 모듈 상태를 `WorldState` 필드로 넣고 `World::step()`에서 모듈 함수를 부른다. 엔진이 자동으로 부르지 않는다.
**모듈 하나 = git 저장소 하나**이고, 공식이든 아니든 같다. 저장소 **`MealKit-Modules`(레지스트리)**에 모듈마다 한 줄 정보 파일을 두면 CI가 검증해서 `index.json`과 검색 사이트를 만든다. 에디터·Manager·`tools\module.ps1`은 그 목록에서 이름으로 찾아 추가한다.
엔진 저장소에는 모듈이 없다. 기준 샘플이 쓰는 모듈의 복사본만 다른 게임처럼 있다.

## 왜 이 방식인가

| 대안 | 안 쓰는 이유 |
|---|---|
| 엔진 기본 기능 | 엔진이 무거워지고 설계 하나를 모든 게임에 강요한다 |
| 공식 모듈을 엔진 저장소 `modules/`에 (초안 1) | 출시 exe에는 추가한 모듈만 들어가지만, 엔진 저장소·배포본에 안 쓰는 기능이 따라오고, "엔진이 고른 답"처럼 보여 다른 사람 모듈과 대우가 달라진다. 공식 모듈의 출시가 엔진 출시에 묶인다 |
| 프리팹만 | 데이터(배치·값)만 담아 `sim/` 로직을 옮길 수 없다 |
| 바이너리 플러그인(DLL) | 출시 exe는 정적 단일 파일이고, `sim/`은 SimCheck를 통과해야 한다 |
| 패키지 매니저(vcpkg·npm식 설치 때 의존 해석) | 오프라인·버전 고정·소스 포함 원칙과 맞지 않고, 게임이 모듈 코드를 고쳐 쓸 수 없다 |
| 업로드 서버(에셋 스토어식: 계정, 파일 올리기, 심사) | 계정·저장소·운영 비용이 들고, 아직 쓸 사람이 없다. 레지스트리로 모자랄 때 다시 본다(보류 표) |

소스 복사라서 얻는 것이 있다. 프로젝트 저장소 하나만 받으면 빌드되고, 게임이 모듈을 마음대로 고친다. SimCheck·해시·스냅샷·디싱크 진단도 그대로 적용된다.
대가는 갱신할 때 고친 파일을 손으로 합쳐야 한다는 것이다(`.new`).

## 모듈 저장소 구조

모듈 저장소의 루트가 곧 모듈 폴더다. 프로젝트 폴더 구조(`docs/CODING_MODEL.md` "프로젝트 폴더 구조")를 반복하고, `tools\check_layout.ps1`이 같은 규칙으로 검사한다.

```
module-cooldown/             (저장소 = 프로젝트에 복사되면 modules/cooldown/)
  module.txt          모듈 정보 (아래)
  LICENSE
  sim/                상태 MEALKIT_STRUCT + 시스템 함수. SimCheck 대상
  view/               그리기, 뷰 모델 채우기, 연출
  assets/<종류>/      .ui, .table, 프리팹, 소리 등. 종류 폴더 규칙은 프로젝트 assets/와 같다
  art/                Penpot 원본 등 (출시·해시 제외)
  docs/               index.md (영어 원문, 한 페이지 틀은 DECISIONS 2026-10-21)
  tests/              모듈 단위 시험 (모듈 CI만 빌드, 게임 빌드는 무시)
  .github/workflows/  레지스트리가 주는 공용 워크플로 한 줄 (아래 "모듈 CI")
```

- 복사하는 것은 모듈 구조뿐이다: `module.txt`, `sim/`, `view/`, `assets/`, `art/`, `docs/`, 루트의 `LICENSE*`·`README*`(`*.new` 제외). 빌드 폴더·`tests/`·`.git`·`.github/`는 가지 않는다. 이때 `module.files`(설치 기록)를 더한다. `module.files`는 첫 줄 `source folder <경로>` 또는 `source git <주소> <태그|-> <커밋> [modules/<이름>]`, 그 뒤 `<sha256>  <모듈 기준 경로>` 줄들이다. 해시는 텍스트 파일이면 줄바꿈과 무관하게, 그 밖은 바이트로 잰다.
- `app/`은 없다. 화면 흐름과 세션은 게임 것이다. 모듈 하나는 기능 하나이고, 큰 기능은 작은 모듈 여럿과 의존으로 쪼갠다.
- 공식 모듈 저장소는 `github.com/MealKit-Engine/module-<이름>` (MIT)이다. 다른 사람 모듈도 이름 규칙만 권장하고 강제하지 않는다.
- 새 모듈은 레지스트리의 템플릿 저장소(`module-template`)로 시작한다.

### `module.txt`

```
@format module 1
name cooldown
version 0.1.0
engine 0.8
title Cooldowns
summary TickTimer bundles for skill cooldowns, with a cooldown icon .ui
tags gameplay timer ui
requires
```

| 키 | 뜻 |
|---|---|
| `name` | 폴더 이름이자 C++ namespace이자 에셋 경로 접두. 소문자·숫자·`_`만. 레지스트리에서 먼저 등록한 쪽이 갖는다 |
| `version` | 모듈의 semver. 저장소 태그 `v<버전>`과 같아야 한다(레지스트리 CI가 검사) |
| `engine` | 맞는 엔진 MINOR. 0.x 동안은 MINOR가 API 경계다 |
| `requires` | 의존 모듈 `이름 최소버전`, 공백으로 구분 |
| `title`·`summary`·`tags` | 목록과 검색에 쓴다 |

## 빌드에 들어가는 방식

`cmake/MealKit.cmake`의 `mealkit_add_game()`이 `source/`와 같은 방식으로 `modules/*/`를 모은다. 게임 `CMakeLists.txt`는 바꾸지 않는다. 모듈은 CMake 파일을 갖지 않으므로, 모듈을 추가해도 configure 때 모듈 코드가 실행되지 않는다.

| 무엇 | 지금 (게임 `source/`) | 모듈에 더할 것 |
|---|---|---|
| 소스 모으기 | `source/{sim,view,app}/**` | `modules/*/{sim,view}/**` |
| include 경로 | `source/` → `#include "sim/World.h"` | `modules/` → `#include "cooldown/sim/Cooldown.h"` |
| SimCheck | `sim/` 파일 | 모듈 `sim/`도 검사한다. 모듈 `sim/`은 엔진 코어, 같은 모듈의 `sim/`, 의존 모듈의 `sim/`만 include한다. 게임 `source/`를 include하면 실패시킨다(모듈은 게임을 모른다) |
| 시뮬레이션 버전 해시 | 게임 `sim/` + 엔진 코어 | 모듈 `sim/`도 넣는다. 모듈 sim 코드가 다른 피어는 접속을 거부한다(지금 규칙 그대로) |
| 헤드리스 타깃 | `sim/` + `Headless.cpp` | 모듈 `sim/`도 넣는다 |
| 핫 리로드 | 게임 DLL | 같은 DLL에 들어가므로 그대로 |

- **namespace**: 모듈 코드는 `namespace <name>`이다(예 `cooldown::`). 레지스트리가 이름을 하나씩만 주므로 모듈끼리는 겹치지 않는다.
- **"자기 상태만 받는다"**: 모듈 시스템 함수의 인자는 모듈 상태 참조, 틱 번호, 모듈 튜닝, 엔진 코어 타입(`Pool`, `Id`, `PhysicsWorld&` 질의 등)뿐이다. 게임에 알릴 일은 반환값이나 모듈 상태 안의 결과 배열로 돌려준다(콜백 등록 없음, `docs/CODING_MODEL.md` "시뮬레이션 안에서 알리기").

### 게임이 모듈을 쓰는 모양

```cpp
// source/sim/World.h
#include "cooldown/sim/Cooldown.h"
MEALKIT_STRUCT(WorldState) {
    ...
    MEALKIT_FIELD(cooldown::Cooldowns, cooldowns, {}, "쿨타임");   // 해시·스냅샷·[상태] 패널에 자동으로 들어간다
};

// source/sim/Tuning.h
MEALKIT_STRUCT(Tuning) {
    ...
    MEALKIT_FIELD(cooldown::Tuning, cooldown, {}, "쿨타임 모듈");  // game.tuning에 "cooldown.slots 8"처럼 들어간다
};

// source/sim/World.cpp — 부르는 순서는 게임이 정한다
void World::step(...) {
    ...
    cooldown::step(state.cooldowns, tick);
    for (const auto& ready : cooldown::readyThisTick(state.cooldowns)) onCooldownReady(ready);
}
```

- **튜닝**: 모듈은 기본값을 담은 자기 `Tuning` 구조체만 낸다. 게임 `Tuning`의 중첩 필드라 따로 파일이 없다. `game.tuning`, [튜닝] 패널, 콘텐츠 해시가 지금 그대로 쓴다(Reflect는 중첩 `MEALKIT_STRUCT`와 `a.b` 경로를 이미 지원한다). 화면용 값은 같은 방식으로 `view.tuning`에 둔다.
- **표**: 모듈에 정의 데이터 표가 필요하면 모듈 폴더의 예시 표를 가리킨다. 예: `MEALKIT_TABLE(ItemDef, "modules/inventory/tables/items.table", 0)`. 게임이 그 표를 고쳐 쓰면 고친 파일이 된다.
- **자동 등록·호출은 없다.** 순서가 게임 코드에 그대로 보여야 결정론 디버깅이 된다. 붙이는 코드는 모듈 문서의 "추가 방법"에 그대로 싣는다.

## 에셋 경로

Export는 `modules/*/assets/`를 출시 폴더 `modules/<이름>/assets/`로 같이 복사한다(`cmake/MealKitExport.cmake`). `.ui`의 `use <이름> modules/<모듈>/ui/x.ui`로 모듈 컴포넌트를 게임 화면에 넣는다.

`modules/<이름>/<종류>/...`로 시작하는 경로는 그 모듈의 `assets/` 아래로 푼다. 예: `modules/health/ui/healthbar.ui` → `<프로젝트>/modules/health/assets/ui/healthbar.ui`.
`assets/` 바로 밑에는 종류 폴더만 올 수 있으므로 프로젝트 에셋과 겹치지 않는다. 경로는 `platform/Paths.h`의 `assetFilePath`(파일 찾기는 `findAssetFile`) 한 곳에서만 푼다(맨 아래 층이라 표·UI·소리·모델·에디터가 모두 쓸 수 있다). 콘텐츠 해시, 에셋 브라우저, 에셋 쿠킹도 같은 함수를 쓴다.
게임이 모듈의 `.ui`를 크게 바꾸고 싶으면 프로젝트 `assets/ui/`에 복사해서 자기 경로를 쓴다. 이렇게 하면 모듈 파일이 고치지 않은 상태로 남아 갱신이 쉽다.

## 레지스트리 `MealKit-Modules`

모듈을 모아 보는 곳이다. 서버 없이 **git 저장소 + CI + 정적 사이트**로 만든다. 이름은 하나의 진실이 있는 곳이다(crates.io-index·Homebrew tap과 같은 방식).

```
MealKit-Modules/
  entries/<이름>.txt       모듈마다 한 파일 (사람이 PR로 더함)
  index.json               CI가 만든 전체 목록 (에디터·Manager·module.ps1이 받는 것)
  site/                    CI가 만든 검색 사이트 (GitHub Pages)
  template/                새 모듈 템플릿 (module-template 저장소의 원본)
  .github/workflows/       PR 검사, 매일 다시 읽기, 모듈 저장소가 부르는 공용 워크플로
```

### 등록 파일 `entries/<이름>.txt`

```
@format module-entry 1
name cooldown
repo https://github.com/MealKit-Engine/module-cooldown
author MealKit
```

- 등록 파일에는 이름과 저장소만 적는다. 버전·요약·의존·엔진은 모듈 저장소의 태그와 `module.txt`에서 CI가 읽는다. 정보가 두 곳에 있지 않게 하려는 것이다.
- **공식 표시**는 등록 파일에 적지 않는다. 저장소 주인이 `MealKit-Engine` 조직이면 공식이다. 적는 칸이 있으면 흉내 낼 수 있기 때문이다.

### CI가 하는 일

| 언제 | 하는 일 |
|---|---|
| 등록 PR | 이름이 비어 있는지와 규칙에 맞는지 본다. 저장소에서 `v*` 태그를 읽고 각 태그의 `module.txt`가 맞는지(이름·버전 일치), `LICENSE`, `docs/index.md`가 있는지 본다. 최신 태그를 **빈 프로젝트에 넣어 `module.txt`의 엔진 버전으로 빌드**하고, SimCheck와 모듈 `tests/`를 돌린다. 실패하면 PR이 막힌다 |
| 매일 | 모든 등록 모듈의 새 태그를 읽어 `index.json`과 사이트를 다시 만든다. 새 태그는 위와 같이 검사하고, 최신 엔진 출시본으로도 빌드해서 "엔진 0.9에서 빌드 실패" 같은 상태를 남긴다 |
| 엔진 출시 전 | 엔진 저장소 CI가 레지스트리의 공용 워크플로로 **공식 모듈 전부를 새 엔진에 빌드**해 본다. API를 바꿔 공식 모듈이 깨지면 출시 전에 안다(초안 1에서 엔진 저장소에 두려던 이유를 이것으로 대신한다) |

### `index.json` (한 모듈 예)

```json
{ "name": "cooldown", "repo": "https://github.com/MealKit-Engine/module-cooldown", "official": true,
  "title": "Cooldowns", "summary": "...", "tags": ["gameplay","timer","ui"], "license": "MIT", "author": "MealKit",
  "versions": [ { "version": "0.1.0", "tag": "v0.1.0", "commit": "3f2a…", "engine": "0.8", "requires": [],
                  "checks": { "0.8": "pass", "0.9": "fail" } } ] }
```

`commit`을 적어 두므로, 누가 태그를 옮기면 받을 때 커밋이 달라서 알아챈다.

### 모아 보는 곳 세 군데 (같은 `index.json`)

| 어디 | 무엇 |
|---|---|
| 사이트 (`mealkit-engine.github.io/MealKit-Modules`) | 검색, 태그, 엔진 버전 필터, 공식만 보기. 모듈마다 `docs/index.md`를 그린 페이지, 버전·빌드 상태, 저장소 링크, 추가 명령 한 줄 |
| MealKit Manager [모듈] 탭 | 사이트와 같은 목록과 검색. 고른 프로젝트에 추가(그 프로젝트 엔진의 `tools\module.ps1`을 부른다). 받은 목록은 `%LOCALAPPDATA%\MealKit\module-index.json`에 캐시하고, 오프라인이면 캐시를 보여 준다 |
| 에디터 [프로젝트 설정] > [모듈] | 설치된 모듈 표(이름, 버전, 새 버전 있음, 출처, 고친 파일 수, 문서), [추가…]는 같은 목록 창. 갱신 미리 보기와 제거 |

공식이 아닌 모듈을 추가할 때는 한 번 경고한다. "이 코드는 에디터와 게임 안에서 실행됩니다. 레지스트리 CI는 빌드만 확인하고 코드를 검토하지 않습니다."

## 추가·갱신·제거 (`tools\module.ps1`)

규칙은 PowerShell 도구 한 곳(`tools\module.ps1`, `project_common.ps1` 재사용)에 둔다. 에디터와 Manager는 이 도구를 부른다(지금 `upgrade_project.ps1`과 같은 관계).

```
tools\module.ps1 search  [<검색어>] [-Tag <태그>]                  레지스트리 목록 (프로젝트 엔진 버전에 맞는 것 먼저)
tools\module.ps1 list    -Project <폴더>                           설치된 모듈, 버전, 고친 파일 수, 새 버전 있음
tools\module.ps1 add     -Project <폴더> <이름[@버전] | git주소#태그 | 폴더>
tools\module.ps1 update  -Project <폴더> <이름> [-To <버전>] [-DryRun] [-Force]
tools\module.ps1 remove  -Project <폴더> <이름>
```

| 동작 | 하는 일 |
|---|---|
| add (이름) | `index.json`에서 프로젝트 엔진 MINOR에 맞는 가장 새 버전을 고른다. `git clone --depth 1 --branch <태그>`로 `%LOCALAPPDATA%\MealKit\module-cache\`에 받고, 커밋이 목록과 같은지 본다. `requires`에서 빠진 모듈은 목록을 보여 준다(`-WithRequires`면 같이 추가). 복사한 뒤 `module.files`에 파일별 sha256과 `source registry cooldown 0.1.0 <커밋>`을 쓰고, 모듈 문서의 "추가 방법"을 연다 |
| add (git 주소·폴더) | 레지스트리에 없는 모듈(만드는 중인 것, 사내 모듈)이다. 같은 방식이고 `source git …`·`source folder …`로 적는다 |
| update | 업그레이드 도구 규칙 그대로다. 고치지 않은 파일(해시가 기록과 같음)은 새것으로 바꾸고, 고친 파일은 남기고 `<파일>.new`를 쓴다. 새 버전에서 없어진 파일은 안 고쳤으면 지우고, 고쳤으면 남기고 알린다 |
| remove | 다른 모듈이 `requires`하면 거절한다. 게임 `source/`에 `"<이름>/`를 include하는 줄이 있으면 그 줄을 보여 주고 거절한다(`-Force`로 강행). 폴더를 지운다(되돌리기는 git이 맡는다) |

- **설치 목록은 `modules/` 폴더 자체다.** `mealkit.project`에 따로 목록을 두지 않는다(ROADMAP 원안의 `modules=` 목록을 바꾼다). 목록과 폴더가 어긋날 일이 없다.
- 모듈은 프로젝트 저장소에 커밋된다. 팀원과 CI는 아무것도 받지 않고 빌드한다. 네트워크는 `search`·`add`·`update` 때만 쓴다.
- 기준 샘플(엔진 저장소 루트의 sandbox 프로젝트)도 다른 게임과 똑같이 `modules/cooldown/` 복사본을 커밋한다. 엔진 저장소에 있는 모듈은 이것뿐이다.

## 모듈 CI (모듈 저장소마다)

모듈 단위 시험은 `tests/CMakeLists.txt`(엔진 `cmake/MealKit.cmake`를 include해 `mealkit_core`에 링크, 빌드 폴더에 `#include "<이름>/..."` 전달 헤더를 만든다) + `tests/*.cpp`(`main`). `cmake -S tests -B build/tests -G Ninja -DMEALKIT_ROOT=<엔진> -DMEALKIT_HEADLESS_ONLY=ON`으로 혼자 빌드한다. 공용 워크플로는 이것을 돌린다.

템플릿에 워크플로 한 줄이 들어 있다. 레지스트리의 공용 워크플로(`uses: MealKit-Engine/MealKit-Modules/.github/workflows/module.yml@v1`)를 부른다.
공용 워크플로는 `module.txt`의 엔진 버전을 받아 빈 프로젝트에 모듈을 넣고 빌드하고, SimCheck와 `tests/`를 돌리고, Windows·Linux에서 결정론 시나리오 해시를 비교한다.
공식 모듈과 다른 사람 모듈이 같은 검사를 쓰므로, 저장소가 여러 개여도 CI는 한 곳에서 고친다.

## 문서

모듈 문서는 모듈 저장소의 `docs/index.md` 한 페이지다. 틀은 그대로다: 개요 / 추가 방법(붙이는 코드) / 사용법 / 동작 원리 / 버전 기록. 프로젝트에 같이 복사되므로 쓰는 버전의 문서가 옆에 남는다.

- **어디서 보나**: 레지스트리 사이트가 모든 모듈 문서를 같은 모양으로 그린다. 공식이든 아니든 같다.
- 문서 저장소(MealKit-docs)의 "게임 개발자 안내 > Modules"는 쓰는 법 한 페이지와 레지스트리 사이트 링크만 둔다. "엔진 개발자 안내 > Writing modules"에는 이 문서의 규칙(폴더, `module.txt`, include 규칙, namespace, 등록 방법)을 옮긴다.
- 이렇게 하면 DECISIONS 2026-10-21의 "문서 빌드가 엔진 `modules/*/docs`를 모은다"를 바꾸게 된다. 한국어는 모듈 저장소의 `docs/index.ko.md`(선택)로 두고, 사이트에서 언어를 고른다.

## 공식 모듈 첫 목록과 순서

| 순서 | 모듈 | 의존 | 비고 |
|---|---|---|---|
| 1 | `cooldown` 쿨타임 | 없음 | 가장 작다. 시스템 전체(저장소, 등록, 추가, 빌드, SimCheck, 튜닝 중첩, `.ui` 경로, 문서, 갱신)를 이것으로 검증한다. sandbox 밀치기 쿨타임을 옮긴다 |
| 2 | `objects` 기본 오브젝트 | 없음 | 트리거 볼륨, 경로 발판, 아이템 스폰 지점 |
| 3 | `procgen` 절차적 생성 | 없음 | 시드 → 배치. 에디터에서 `.scene`으로 굽는다 |
| 4 | `health` 체력·데미지 | 게임 흐름 프레임워크 | 사망 → 리스폰이 매치 상태 머신과 엮인다 |
| 5 | `inventory` 인벤토리 | 없음 (`.table`) | 세이브·매치 시작 데이터와 연결한다 |
| 6 | `minimap` 미니맵 | 없음 | `view/`만 있는 모듈의 예 |

## 구현 순서 (항목 안에서)

1. 엔진: `module.txt` 읽기. `mealkit_add_game()`이 모듈을 모은다(소스, include, SimCheck, 시뮬레이션 버전, 헤드리스). SimCheck에 "모듈은 `source/`를 모른다" 규칙을 더한다
2. 엔진: 에셋 경로 `modules/<이름>/...` 풀기, `check_layout.ps1`에 모듈 폴더, 콘텐츠 해시
3. `tools\module.ps1`: 폴더·git 주소로 add/update/remove/list, 스크립트 시험
4. 모듈 템플릿 + `cooldown` 저장소(로컬): 코드, `tests/`, 문서. sandbox에 add해서 `mp_test` PASS, 해시가 같은지 확인
5. 레지스트리 저장소: `entries/`, 공용 워크플로(모듈 CI), PR 검사, `index.json` 만들기
6. `module.ps1 search`와 이름으로 add, Manager [모듈] 탭, 에디터 [모듈] 창
7. 레지스트리 사이트(정적 페이지: 검색, 필터, 문서 그리기)
8. 문서 저장소: Modules 안내 페이지, Writing modules(영어·한국어)
9. 나머지 공식 모듈은 위 표 순서로, 각각 자기 저장소에서

1~4는 GitHub 없이 로컬에서 끝난다. 5부터는 GitHub 저장소(`MealKit-Modules`, `module-cooldown`)가 필요하다. 저장소 만들기와 push는 사용자가 한다(오디오 2와 같음).

## 정한 것 (2026-10-28)

1. 모듈 하나 = 저장소 하나 (공식 모듈만 한 저장소에 모으지 않는다)
2. 레지스트리는 git 저장소 + CI + GitHub Pages (토픽 검색만, 업로드 서버는 아님 — 서버는 보류 표)
3. 등록 PR은 CI 통과 뒤 사람이 합친다
4. 설치 목록은 `modules/` 폴더 자체 (`mealkit.project`에 목록 없음)
5. 에셋 경로 접두 `modules/<이름>/...`, namespace는 모듈 이름
6. 모듈 문서는 레지스트리 사이트, MealKit-docs는 안내 한 페이지와 링크. 한국어는 `index.ko.md`
