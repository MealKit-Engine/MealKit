#pragma once

#include "mealkit/core/Math.h"

#include <mkaudio/Mixer.h>

#include <deque>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

// Sound (docs/CODING_MODEL.md "How to play sounds"): a sound event name + one parameter.
//   ctx.audio.play("hit", position, {{"speed", 6.0f}});   // view/ only. sim/ only leaves tick events (SimCheck blocks audio/ headers)
// Events are in assets/sounds.events (edited by the authoring tool MealKit Audio, format in runtime mkaudio/Events.h): clips, containers (random / switch / blend),
// random volume/pitch, parameter curves, attenuation distance, buses, priority, concurrency limit and what to cut on overflow, cooldown, subtitles.
// View-only (outside the content hash), so edits during play are heard immediately.
// Voice management (cap on actually mixed voices, virtual voices, same-frame grouping) is the engine's job. The game doesn't choose.
// Layers (docs/DECISIONS.md 2026-10-26 audio runtime): mixing, voices, buses, containers, and the event file format are done by the runtime mkaudio
// (separate repo MealKit-Audio, third_party/mkaudio), and this folder is the glue layer: finding and reloading files, clip asset paths, logging,
// profile markers, live connection service audio/ (the authoring tool plays in a running game and sees state).
namespace mealkit::live {
class Server;
}

namespace mealkit::audio {

inline constexpr const char* kEventsFile = "sounds.events";    // relative to assets
inline constexpr const char* kOldSoundTable = "sounds.table"; // format up to 0.5.1: if still present, read with a warning (upgrade_project migrates it)

// Use the runtime's value types as is (playback handle, parameters, event definitions, [Sound] panel info)
using Param = mkaudio::Param;
using Handle = mkaudio::Handle;     // one playing sound. Becomes invalid when it ends or is cut (later calls are ignored)
using EventDef = mkaudio::EventDef;
using VoiceInfo = mkaudio::VoiceInfo;
using CullInfo = mkaudio::CullInfo; // reason is an English word from the runtime (the editor [Sound] panel translates it)
using BusInfo = mkaudio::BusInfo;
using Subtitle = mkaudio::Subtitle; // a sound with a subtitle played (same keys merge into one line and extend the time)
using Stats = mkaudio::Stats;

struct EngineDesc {
    u32 maxVoices = 32; // number of actually mixed voices (project setting audioVoices)
    bool muted = false; // muted from the start (join windows of editor multiplayer play)
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Opens the default output device. false if there's no device (all later calls do nothing — the game runs without sound).
    bool init(const EngineDesc& desc = {});
    void shutdown();
    bool ready() const;

    // Plays a sound event. The actual start happens in update() (if the same event is played many times in one frame, up to the limit, loudest first).
    Handle play(std::string_view event, Vec3 position, std::initializer_list<Param> params = {});
    Handle play2D(std::string_view event, std::initializer_list<Param> params = {}); // no position (UI, background music)
    void setParam(Handle handle, std::string_view name, f32 value);
    void setPosition(Handle handle, Vec3 position);
    void stop(Handle handle, f32 fadeSeconds = 0.0f);
    void stopAll();
    bool playing(Handle handle) const;
    // Value used when an event looks up an unknown parameter (e.g. "music_intensity")
    void setGlobalParam(std::string_view name, f32 value);

    void setListener(Vec3 position, Vec3 forward); // listener = camera (the engine calls this every frame via FrameParams)
    Vec3 listener() const;
    void setBusVolume(std::string_view bus, f32 volume);
    f32 busVolume(std::string_view bus) const;
    void setMuted(bool muted); // when the window loses focus (setting "Mute when unfocused")
    // Subtitles to show now (in order played, regardless of whether the sound is actually audible: shown even when muted or at volume 0)
    const std::vector<Subtitle>& subtitles() const;
    bool muted() const;

    // Once per frame: reload the event file (if changed), handle play requests, manage voices, clean up finished sounds
    void update(f64 dt);
    // Reload the event file now (audio/reload after the authoring tool saves)
    void reloadEvents();
    const std::string& eventsPath() const; // file that was read (empty string if none)

    // Editor [Sound] panel
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

// Live connection service audio/ (used by MealKit Audio, dev build games and the editor):
//   POST audio/play     body "event=<name>&p.<param>=<value>..." → play at the listener
//   POST audio/reload   reload the event file now
//   GET  audio/state    {"file","events","stats":{...},"voices":[...],"buses":[...]}
// engine() is called per request and returns the audio currently in use (nullptr if none → 503)
void addLiveService(live::Server& server, std::function<Engine*()> engine);

} // namespace mealkit::audio
