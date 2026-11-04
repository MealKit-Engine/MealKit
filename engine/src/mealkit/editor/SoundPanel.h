#pragma once

#include "mealkit/audio/Audio.h"

#include <string>
#include <vector>

// Editor [Sound] panel: sound event preview with parameter sliders, real/virtual voice list, culled sounds and reasons, bus levels and volumes.
// The events themselves are edited in assets/sounds.events by the authoring tool MealKit Audio ([Tools] menu) (saving makes audio reread them, so they're heard immediately even during play).
namespace mealkit::editor {

class SoundPanel {
public:
    void drawPreview(audio::Engine& audio); // [Data] page: try sound events
    void drawMonitor(audio::Engine& audio); // [Debug] page: buses, voices, culled plays

private:
    std::string m_event;
    f32 m_param = 0.0f;
    audio::Handle m_preview;
    std::vector<audio::VoiceInfo> m_voices;
    std::vector<audio::BusInfo> m_buses;
};

} // namespace mealkit::editor
