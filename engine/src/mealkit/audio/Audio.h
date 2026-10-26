#pragma once

#include "mealkit/core/Math.h"

#include <mkaudio/Mixer.h>

#include <deque>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// 소리 (docs/CODING_MODEL.md "소리를 내는 방법"): 소리 이벤트 이름 + 파라미터 하나.
//   ctx.audio.play("hit", position, {{"speed", 6.0f}});   // view/에서만. sim/은 틱 이벤트만 남긴다 (SimCheck가 audio/ 헤더를 막는다)
// 이벤트는 assets/sounds.events (저작 도구 MealKit Audio가 고친다, 형식은 런타임 mkaudio/Events.h): 클립·컨테이너(무작위 / 스위치 / 블렌드),
// 무작위 볼륨·피치, 파라미터 곡선, 감쇠 거리, 버스, 우선순위, 동시 재생 상한과 넘칠 때 끊을 것, 쿨다운, 자막.
// 화면 전용(콘텐츠 해시 밖)이라 플레이 중에 고치면 바로 들린다.
// 목소리 관리(실제로 믹싱하는 수 상한, 가상 목소리, 같은 프레임 묶기)는 엔진이 맡는다. 게임은 고르지 않는다.
// 층 (docs/DECISIONS.md 2026-10-26 오디오 런타임): 섞기·목소리·버스·컨테이너·이벤트 파일 형식은 런타임 mkaudio
// (별도 저장소 MealKit-Audio, third_party/mkaudio)가 하고, 이 폴더는 접착층이다: 파일 찾기와 다시 읽기, 클립 에셋 경로, 로그,
// 프로파일 마커, 라이브 연결 서비스 audio/ (저작 도구가 실행 중인 게임에서 재생하고 상태를 본다).
namespace mealkit::live {
class Server;
}

namespace mealkit::audio {

inline constexpr const char* kEventsFile = "sounds.events";    // assets 기준
inline constexpr const char* kOldSoundTable = "sounds.table"; // 0.5.1까지의 형식: 아직 있으면 읽고 경고한다 (upgrade_project가 옮긴다)

// 런타임의 값 타입을 그대로 쓴다 (재생 핸들, 파라미터, 이벤트 정의, [소리] 패널 정보)
using Param = mkaudio::Param;
using Handle = mkaudio::Handle;     // 재생 중인 소리 하나. 끝나거나 끊기면 무효가 된다(그 뒤 호출은 무시된다)
using EventDef = mkaudio::EventDef;
using VoiceInfo = mkaudio::VoiceInfo;
using CullInfo = mkaudio::CullInfo; // reason은 런타임의 영어 낱말 (에디터 [소리] 패널이 번역한다)
using BusInfo = mkaudio::BusInfo;
using Subtitle = mkaudio::Subtitle; // 자막이 있는 소리가 났다 (같은 키는 한 줄로 합치고 시간을 늘린다)
using Stats = mkaudio::Stats;

struct EngineDesc {
    u32 maxVoices = 32; // 실제로 믹싱하는 목소리 수 (프로젝트 설정 audioVoices)
    bool muted = false; // 처음부터 음소거 (에디터 여러 명 플레이의 참가 창)
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // 기본 출력 장치를 연다. 장치가 없으면 false (그 뒤 호출은 모두 아무 일도 하지 않는다 — 게임은 소리 없이 돈다).
    bool init(const EngineDesc& desc = {});
    void shutdown();
    bool ready() const;

    // 소리 이벤트 재생. 실제 시작은 update()에서 (같은 프레임에 같은 이벤트가 여러 번이면 상한만큼, 세기 큰 순).
    Handle play(std::string_view event, Vec3 position, std::initializer_list<Param> params = {});
    Handle play2D(std::string_view event, std::initializer_list<Param> params = {}); // 위치 없음 (UI, 배경음)
    void setParam(Handle handle, std::string_view name, f32 value);
    void setPosition(Handle handle, Vec3 position);
    void stop(Handle handle, f32 fadeSeconds = 0.0f);
    void stopAll();
    bool playing(Handle handle) const;
    // 이벤트가 모르는 파라미터를 찾을 때 쓰는 값 (예: "music_intensity")
    void setGlobalParam(std::string_view name, f32 value);

    void setListener(Vec3 position, Vec3 forward); // 리스너 = 카메라 (엔진이 매 프레임 FrameParams로 부른다)
    Vec3 listener() const;
    void setBusVolume(std::string_view bus, f32 volume);
    f32 busVolume(std::string_view bus) const;
    void setMuted(bool muted); // 창이 포커스를 잃었을 때 (설정 "포커스를 잃으면 음소거")
    // 지금 보여 줄 자막 (나온 순서, 소리가 실제로 들리는지와 무관하다: 음소거·볼륨 0이어도 보인다)
    const std::vector<Subtitle>& subtitles() const;
    bool muted() const;

    // 매 프레임 한 번: 이벤트 파일 다시 읽기(바뀌면), 재생 요청 처리, 목소리 관리, 끝난 소리 정리
    void update(f64 dt);
    // 이벤트 파일을 지금 다시 읽는다 (저작 도구가 저장한 뒤 audio/reload)
    void reloadEvents();
    const std::string& eventsPath() const; // 읽은 파일 (없으면 빈 문자열)

    // 에디터 [소리] 패널
    std::vector<std::string> eventNames() const;
    const EventDef* event(std::string_view name) const;
    void voices(std::vector<VoiceInfo>& out) const;
    const std::deque<CullInfo>& culls() const;
    void buses(std::vector<BusInfo>& out) const;
    const Stats& stats() const;

private:
    struct Impl;
    Impl* m = nullptr;
};

// 라이브 연결 서비스 audio/ (MealKit Audio가 쓴다, 개발 빌드 게임과 에디터):
//   POST audio/play     본문 "event=<이름>&p.<파라미터>=<값>..." → 리스너 자리에서 재생
//   POST audio/reload   이벤트 파일을 지금 다시 읽는다
//   GET  audio/state    {"file","events","stats":{...},"voices":[...],"buses":[...]}
// engine()은 요청마다 불러 지금 쓰는 오디오를 돌려준다 (없으면 nullptr → 503)
void addLiveService(live::Server& server, std::function<Engine*()> engine);

} // namespace mealkit::audio
