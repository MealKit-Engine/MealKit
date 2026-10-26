#pragma once

#include "mealkit/audio/Audio.h"

#include <string>
#include <vector>

// 에디터 [소리] 패널: 소리 이벤트 미리듣기와 파라미터 슬라이더, 실제·가상 목소리 목록, 끊긴 소리와 이유, 버스 레벨과 볼륨.
// 이벤트 자체는 저작 도구 MealKit Audio([도구] 메뉴)가 assets/sounds.events에서 고친다 (저장하면 오디오가 다시 읽어 플레이 중에도 바로 들린다).
namespace mealkit::editor {

class SoundPanel {
public:
    void draw(audio::Engine& audio, bool* open);

private:
    std::string m_event;
    f32 m_param = 0.0f;
    audio::Handle m_preview;
    std::vector<audio::VoiceInfo> m_voices;
    std::vector<audio::BusInfo> m_buses;
};

} // namespace mealkit::editor
