#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"

#include <mkaudio/Mixer.h>

#include <deque>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// 소리 (docs/CODING_MODEL.md "소리를 내는 방법"): 소리 이벤트 이름 + 파라미터 하나.
//   ctx.audio.play("hit", position, {{"speed", 6.0f}});   // view/에서만. sim/은 틱 이벤트만 남긴다 (SimCheck가 audio/ 헤더를 막는다)
// 이벤트는 assets/sounds.table의 행(SoundEvent): 클립·컨테이너(무작위 / 스위치 / 블렌드), 무작위 볼륨·피치, 파라미터 곡선,
// 감쇠 거리, 버스, 우선순위, 동시 재생 상한과 넘칠 때 끊을 것, 쿨다운. 표는 화면 전용(콘텐츠 해시 밖)이라 플레이 중에 고치면 바로 들린다.
// 목소리 관리(실제로 믹싱하는 수 상한, 가상 목소리, 같은 프레임 묶기)는 엔진이 맡는다. 게임은 고르지 않는다.
// 층 (docs/DECISIONS.md 2026-10-26 오디오 런타임): 섞기·목소리·버스·컨테이너는 엔진 밖 런타임 mkaudio(engine/libs/mkaudio, MealKit 헤더 없음)가 하고,
// 이 폴더는 접착층이다: 표 읽기와 다시 읽기, SoundEvent(Reflect 행 타입) → mkaudio::EventDef, 에셋 경로 찾기, 로그, 프로파일 마커.
namespace mealkit::audio {

// 소리 이벤트 한 행 (assets/sounds.table). 필드 설명은 에디터 글자라 영어, 한국어는 engine/res/strings/ko.txt.
MEALKIT_STRUCT(SoundEvent) {
    MEALKIT_FIELD(std::string, clips, {}, "Clips (assets paths, separated by spaces)");
    MEALKIT_FIELD(std::string, container, "random", "Container (random, switch, blend)");
    MEALKIT_FIELD(std::string, param, {}, "Parameter (switch, blend and curve)");
    MEALKIT_FIELD(f32, paramMin, 0.0f, "Parameter at curve start");
    MEALKIT_FIELD(f32, paramMax, 1.0f, "Parameter at curve end");
    MEALKIT_FIELD(f32, volume, 1.0f, "Volume");
    MEALKIT_FIELD(f32, volumeRandom, 0.0f, "Random volume (plus or minus, ratio)");
    MEALKIT_FIELD(f32, pitch, 1.0f, "Pitch");
    MEALKIT_FIELD(f32, pitchRandom, 0.0f, "Random pitch (plus or minus, ratio)");
    MEALKIT_FIELD(f32, volumeAtMin, 1.0f, "Curve: volume at start");
    MEALKIT_FIELD(f32, volumeAtMax, 1.0f, "Curve: volume at end");
    MEALKIT_FIELD(f32, pitchAtMin, 1.0f, "Curve: pitch at start");
    MEALKIT_FIELD(f32, pitchAtMax, 1.0f, "Curve: pitch at end");
    MEALKIT_FIELD(f32, lowpassAtMin, 0.0f, "Curve: low-pass at start (Hz, 0 is off)");
    MEALKIT_FIELD(f32, lowpassAtMax, 0.0f, "Curve: low-pass at end (Hz, 0 is off)");
    MEALKIT_FIELD(bool, spatial, true, "3D position");
    MEALKIT_FIELD(f32, minDistance, 2.0f, "Full volume within (m)");
    MEALKIT_FIELD(f32, maxDistance, 40.0f, "Silent beyond (m)");
    MEALKIT_FIELD(std::string, bus, "sfx", "Bus (music, sfx, ui, voice or a project bus)");
    MEALKIT_FIELD(bool, loop, false, "Loop");
    MEALKIT_FIELD(bool, stream, false, "Stream from disk (music)");
    MEALKIT_FIELD(u32, priority, 50, "Priority (higher keeps playing)");
    MEALKIT_FIELD(u32, maxInstances, 8, "Max instances at once");
    MEALKIT_FIELD(std::string, steal, "oldest", "When full (oldest, quietest, none)");
    MEALKIT_FIELD(f32, cooldown, 0.0f, "Cooldown (s)");
    // 자막 (접근성, 플레이어 설정 gameplay.subtitles): 게임 문자열 표 키. 소리가 날 때 화면 아래 표준 자막(ui/subtitles.ui)에 보인다
    MEALKIT_FIELD(std::string, subtitle, {}, "Subtitle (game string key, empty is none)");
    MEALKIT_FIELD(f32, subtitleSeconds, 2.5f, "Subtitle shown for (s)");
};

inline constexpr const char* kSoundTable = "sounds.table"; // assets 기준

// 엔진이 등록하는 표 (에디터 [표] 패널): 소리 이벤트 (화면 전용)
const reflect::TableList& engineTables();

// 런타임의 값 타입을 그대로 쓴다 (재생 핸들, 파라미터, [소리] 패널 정보)
using Param = mkaudio::Param;
using Handle = mkaudio::Handle;     // 재생 중인 소리 하나. 끝나거나 끊기면 무효가 된다(그 뒤 호출은 무시된다)
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
    void setBusVolume(std::string_view bus, f32 volume);
    f32 busVolume(std::string_view bus) const;
    void setMuted(bool muted); // 창이 포커스를 잃었을 때 (설정 "포커스를 잃으면 음소거")
    // 지금 보여 줄 자막 (나온 순서, 소리가 실제로 들리는지와 무관하다: 음소거·볼륨 0이어도 보인다)
    const std::vector<Subtitle>& subtitles() const;
    bool muted() const;

    // 매 프레임 한 번: 표 다시 읽기(파일이 바뀌면), 재생 요청 처리, 목소리 관리, 끝난 소리 정리
    void update(f64 dt);
    // 소리 이벤트 표를 지금 다시 읽는다 (에디터가 저장한 뒤)
    void reloadEvents();

    // 에디터 [소리] 패널
    std::vector<std::string> eventNames() const;
    const SoundEvent* event(std::string_view name) const;
    void voices(std::vector<VoiceInfo>& out) const;
    const std::deque<CullInfo>& culls() const;
    void buses(std::vector<BusInfo>& out) const;
    const Stats& stats() const;

private:
    struct Impl;
    Impl* m = nullptr;
};

} // namespace mealkit::audio
