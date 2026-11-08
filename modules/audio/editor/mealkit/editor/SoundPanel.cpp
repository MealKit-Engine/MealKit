#include "mealkit/editor/SoundPanel.h"
#include "mealkit/editor/FieldEditor.h"

#include "mealkit/editor/Strings.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <imgui.h>

namespace mealkit::editor {
namespace {

constexpr const char* kSoundsWindow = "Sounds###Sounds";

// audio::CullInfo::reason (English word from the engine) → display text
const char* reasonText(const char* r) {
    const std::string s = r;
    if (s == "cooldown") return tr("cooldown");
    if (s == "same frame") return tr("same frame");
    if (s == "max instances") return tr("max instances");
    if (s == "max instances (stolen)") return tr("max instances (stolen)");
    if (s == "voice limit") return tr("voice limit");
    if (s == "inaudible") return tr("inaudible");
    if (s == "missing event") return tr("missing event");
    if (s == "missing clip") return tr("missing clip");
    return r;
}

} // namespace

void SoundPanel::drawPreview(audio::Engine& audio) {
    if (!audio.ready()) {
        ImGui::TextDisabled("%s", tr("No audio output device."));
        return;
    }
    // ---- Preview
    ImGui::SeparatorText(tr("Preview"));
    const std::vector<std::string> names = audio.eventNames();
    if (m_event.empty() && !names.empty()) m_event = names[0];
    ImGui::SetNextItemWidth(220.0f);
    if (beginCombo("##event", m_event.c_str())) {
        for (const std::string& n : names)
            if (ImGui::Selectable(n.c_str(), n == m_event)) m_event = n;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const audio::EventDef* def = audio.event(m_event);
    if (ImGui::Button(tr("Play")) && def) {
        audio.stop(m_preview);
        m_preview = audio.play2D(m_event, {{def->param.c_str(), m_param}});
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Stop"))) audio.stop(m_preview, 0.2f);
    if (def && !def->param.empty()) { // curve/switch/blend parameter
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat(def->param.c_str(), &m_param, std::min(def->paramMin, def->paramMax),
                               std::max(def->paramMin, def->paramMax)))
            audio.setParam(m_preview, def->param, m_param);
    }
    if (def) {
        std::string clips;
        for (const mkaudio::ClipRef& c : def->clips) clips += (clips.empty() ? "" : " ") + c.name;
        ImGui::TextDisabled("%s", trf("%s | bus %s%s", clips.c_str(), def->bus.c_str(), def->loop ? " | loop" : "").c_str());
    }
    ImGui::TextDisabled("%s", tr("Edit sound events in MealKit Audio (Tools menu)."));
}

void SoundPanel::drawMonitor(audio::Engine& audio) {
    if (!audio.ready()) {
        ImGui::TextDisabled("%s", tr("No audio output device."));
        return;
    }
    const audio::Stats& st = audio.stats();
    ImGui::Text("%s", trf("Voices: %u mixed, %u virtual | %.3f ms per frame", st.realVoices, st.virtualVoices, st.updateMs).c_str());

    // ---- Buses
    ImGui::SeparatorText(tr("Buses"));
    audio.buses(m_buses);
    for (const audio::BusInfo& b : m_buses) {
        ImGui::PushID(b.name.c_str());
        f32 volume = b.volume;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat(b.name.c_str(), &volume, 0.0f, 1.5f, "%.2f")) audio.setBusVolume(b.name, volume);
        ImGui::SameLine(260.0f);
        char overlay[32];
        std::snprintf(overlay, sizeof(overlay), "%.2f", b.peak);
        ImGui::ProgressBar(std::min(b.peak, 1.0f), ImVec2(-FLT_MIN, 0.0f), overlay);
        ImGui::PopID();
    }

    // ---- Voices
    ImGui::SeparatorText(tr("Voices"));
    audio.voices(m_voices);
    std::sort(m_voices.begin(), m_voices.end(), [](const audio::VoiceInfo& a, const audio::VoiceInfo& b) {
        if (a.virtualVoice != b.virtualVoice) return !a.virtualVoice;
        return a.audibility > b.audibility;
    });
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##voices", 4, flags, ImVec2(0.0f, 180.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(tr("Event"));
        ImGui::TableSetupColumn(tr("State"));
        ImGui::TableSetupColumn(tr("Audibility"));
        ImGui::TableSetupColumn(tr("Distance"));
        ImGui::TableHeadersRow();
        for (const audio::VoiceInfo& v : m_voices) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v.event.c_str());
            ImGui::TableNextColumn();
            if (v.virtualVoice) ImGui::TextDisabled("%s", tr("virtual"));
            else ImGui::TextUnformatted(v.loop ? tr("loop") : tr("one-shot"));
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", v.audibility);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f m", v.distance);
        }
        ImGui::EndTable();
    }

    // ---- Culled sounds
    ImGui::SeparatorText(tr("Culled (newest first)"));
    const std::deque<audio::CullInfo>& culls = audio.culls();
    if (ImGui::BeginChild("##culls", ImVec2(0.0f, 0.0f))) {
        for (auto it = culls.rbegin(); it != culls.rend(); ++it)
            ImGui::TextDisabled("%s  %s", it->event.c_str(), reasonText(it->reason));
    }
    ImGui::EndChild();
}

} // namespace mealkit::editor
