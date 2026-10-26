#include "mealkit/audio/Audio.h"

#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"
#include "mealkit/data/Tables.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>

// 접착층: 섞기·목소리 관리는 런타임 mkaudio::Mixer (engine/libs/mkaudio). 여기서는 표를 읽어 넘기고, 에셋 경로·로그·프로파일을 잇는다.
namespace mealkit::audio {
namespace {

constexpr f64 kTableCheckSeconds = 0.25;

mkaudio::Vec3 toMk(Vec3 v) { return {v.x, v.y, v.z}; }

std::vector<std::string> splitClips(const std::string& text) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        size_t j = i;
        while (j < text.size() && text[j] != ' ' && text[j] != '\t') ++j;
        if (j > i) out.push_back(text.substr(i, j - i));
        i = j;
    }
    return out;
}

} // namespace

struct Engine::Impl {
    mkaudio::Mixer mixer;
    sim::Table<SoundEvent> table; // event()가 돌려주는 행
    std::vector<std::string> missingClips; // 이미 경고한 클립 (다시 읽을 때 또 쓰지 않는다)
    f64 time = 0.0;
    f64 tableCheck = 0.0;
    std::string tablePath;
    unsigned long long tableTime = 0;

    // 표 행 → 런타임 이벤트. 클립은 assets 기준 경로를 파일로 찾아 넘긴다
    mkaudio::EventDef convert(const std::string& name, const SoundEvent& d) {
        mkaudio::EventDef e;
        e.name = name;
        for (const std::string& path : splitClips(d.clips)) {
            mkaudio::ClipRef c{path, findAsset("assets/" + path)};
            if (c.file.empty() && std::find(missingClips.begin(), missingClips.end(), path) == missingClips.end()) {
                MEALKIT_WARN("Audio: clip assets/%s not found", path.c_str());
                missingClips.push_back(path);
            }
            e.clips.push_back(std::move(c));
        }
        e.container = d.container == "blend" ? mkaudio::Container::Blend
                      : d.container == "switch" ? mkaudio::Container::Switch
                                                : mkaudio::Container::Random;
        if (d.container != "random" && d.container != "blend" && d.container != "switch")
            MEALKIT_WARN("Audio: event %s: unknown container '%s' (random, switch, blend)", name.c_str(), d.container.c_str());
        e.param = d.param;
        e.paramMin = d.paramMin;
        e.paramMax = d.paramMax;
        e.volume = d.volume;
        e.volumeRandom = d.volumeRandom;
        e.pitch = d.pitch;
        e.pitchRandom = d.pitchRandom;
        e.volumeAtMin = d.volumeAtMin;
        e.volumeAtMax = d.volumeAtMax;
        e.pitchAtMin = d.pitchAtMin;
        e.pitchAtMax = d.pitchAtMax;
        e.lowpassAtMin = d.lowpassAtMin;
        e.lowpassAtMax = d.lowpassAtMax;
        e.spatial = d.spatial;
        e.minDistance = d.minDistance;
        e.maxDistance = d.maxDistance;
        e.bus = d.bus.empty() ? "sfx" : d.bus;
        e.loop = d.loop;
        e.stream = d.stream;
        e.priority = d.priority;
        e.maxInstances = d.maxInstances;
        e.steal = d.steal == "quietest" ? mkaudio::Steal::Quietest : d.steal == "none" ? mkaudio::Steal::None : mkaudio::Steal::Oldest;
        e.cooldown = d.cooldown;
        e.subtitle = d.subtitle;
        e.subtitleSeconds = d.subtitleSeconds;
        return e;
    }

    void loadEvents() {
        tablePath = findAsset(std::string("assets/") + kSoundTable);
        tableTime = tablePath.empty() ? 0 : fileWriteTime(tablePath);
        table = {};
        if (!tablePath.empty()) data::loadTable(table, kSoundTable);
        std::vector<mkaudio::EventDef> defs;
        for (u32 i = 0; i < table.size(); ++i) defs.push_back(convert(table.ids[i], table.rows[i]));
        mixer.setEvents(std::move(defs));
        MEALKIT_INFO("Audio: %zu sound event(s) from %s", static_cast<size_t>(table.size()),
                     tablePath.empty() ? "(no sounds.table)" : tablePath.c_str());
    }

    void pollTable() {
        if (time < tableCheck) return;
        tableCheck = time + kTableCheckSeconds;
        const std::string path = findAsset(std::string("assets/") + kSoundTable);
        const unsigned long long t = path.empty() ? 0 : fileWriteTime(path);
        if (path != tablePath || t != tableTime) loadEvents();
    }
};

const reflect::TableList& engineTables() {
    static const reflect::TableList list{{&reflect::typeOf<SoundEvent>(), kSoundTable, reflect::kTableView}};
    return list;
}

Engine::Engine() : m(new Impl()) {}

Engine::~Engine() {
    shutdown();
    delete m;
}

bool Engine::init(const EngineDesc& desc) {
    shutdown();
    mkaudio::Desc d;
    d.maxVoices = desc.maxVoices;
    d.muted = desc.muted;
    d.log = [](mkaudio::LogLevel level, const std::string& text) {
        if (level == mkaudio::LogLevel::Warn) MEALKIT_WARN("Audio: %s", text.c_str());
        else MEALKIT_INFO("Audio: %s", text.c_str());
    };
    if (!m->mixer.init(d)) return false;
    m->loadEvents();
    MEALKIT_INFO("Audio: %u Hz, %u channel(s), up to %u voices", m->mixer.sampleRate(), m->mixer.channels(), m->mixer.maxVoices());
    return true;
}

void Engine::shutdown() {
    m->mixer.shutdown();
    m->table = {};
}

bool Engine::ready() const { return m->mixer.ready(); }

Handle Engine::play(std::string_view event, Vec3 position, std::initializer_list<Param> params) {
    return m->mixer.play(event, toMk(position), params);
}

Handle Engine::play2D(std::string_view event, std::initializer_list<Param> params) { return m->mixer.play2D(event, params); }

void Engine::setParam(Handle h, std::string_view name, f32 value) { m->mixer.setParam(h, name, value); }

void Engine::setPosition(Handle h, Vec3 position) { m->mixer.setPosition(h, toMk(position)); }

void Engine::stop(Handle h, f32 fadeSeconds) { m->mixer.stop(h, fadeSeconds); }

void Engine::stopAll() { m->mixer.stopAll(); }

bool Engine::playing(Handle h) const { return m->mixer.playing(h); }

void Engine::setGlobalParam(std::string_view name, f32 value) { m->mixer.setGlobalParam(name, value); }

void Engine::setListener(Vec3 position, Vec3 forward) { m->mixer.setListener(toMk(position), toMk(forward)); }

void Engine::setBusVolume(std::string_view name, f32 volume) { m->mixer.setBusVolume(name, volume); }

f32 Engine::busVolume(std::string_view name) const { return m->mixer.busVolume(name); }

void Engine::setMuted(bool muted) { m->mixer.setMuted(muted); }

bool Engine::muted() const { return m->mixer.muted(); }

void Engine::update(f64 dt) {
    if (!m->mixer.ready()) return;
    MEALKIT_ZONE("Audio");
    m->time += dt;
    m->pollTable();
    m->mixer.update(dt);
}

const std::vector<Subtitle>& Engine::subtitles() const { return m->mixer.subtitles(); }

void Engine::reloadEvents() {
    if (m->mixer.ready()) m->loadEvents();
}

std::vector<std::string> Engine::eventNames() const { return m->mixer.eventNames(); }

const SoundEvent* Engine::event(std::string_view name) const {
    for (u32 i = 0; i < m->table.size(); ++i)
        if (m->table.ids[i] == name) return &m->table.rows[i];
    return nullptr;
}

void Engine::voices(std::vector<VoiceInfo>& out) const { m->mixer.voices(out); }

const std::deque<CullInfo>& Engine::culls() const { return m->mixer.culls(); }

void Engine::buses(std::vector<BusInfo>& out) const { m->mixer.buses(out); }

const Stats& Engine::stats() const { return m->mixer.stats(); }

} // namespace mealkit::audio
