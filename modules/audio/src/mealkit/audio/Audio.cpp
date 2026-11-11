#include "mealkit/audio/Audio.h"

#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"
#include "mealkit/live/Server.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/platform/Pack.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/scene/Scene.h"

#include <mkaudio/Events.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// Glue layer: mixing, voice management, and the event file format are the mkaudio runtime (third_party/mkaudio). Here we find and pass files, and wire up asset paths, logging, profiling, and the live connection.
namespace mealkit::audio {
namespace {

constexpr f64 kFileCheckSeconds = 0.25;
constexpr f32 kOcclusionNear = 1.5f; // the ray stops this far before the sound: the body making it (a character, a crate) doesn't hide it

mkaudio::Vec3 toMk(Vec3 v) { return {v.x, v.y, v.z}; }

// assets/sounds.table up to 0.5.1: key names are the same, so changing only "row <name>" to "event <name>" gives the event format
std::string tableToEvents(const std::string& table) {
    std::string out = std::string("@format ") + mkaudio::kEventsFormatKind + " 1\n";
    size_t pos = 0;
    while (pos < table.size()) {
        size_t end = table.find('\n', pos);
        if (end == std::string::npos) end = table.size();
        std::string line = table.substr(pos, end - pos);
        pos = end + 1;
        if (line.rfind("@format", 0) == 0) continue;
        if (line.rfind("row ", 0) == 0) line = "event " + line.substr(4);
        out += line + "\n";
    }
    return out;
}

} // namespace

struct Engine::Impl {
    mkaudio::Mixer mixer;
    mkaudio::EventFile file; // definitions returned by event() (clip files are resolved paths)
    std::vector<std::string> missingClips; // clips already warned about (not repeated on reload)
    f64 time = 0.0;
    f64 fileCheck = 0.0;
    std::string path; // file that was read
    unsigned long long fileTime = 0;
    bool oldTable = false;
    Vec3 listener;
    const physics::PhysicsWorld* world = nullptr;
    u32 rayCursor = 0;
    std::vector<mkaudio::Emitter> emitters;
    std::vector<ReverbZone> zones;
    i32 zone = -2; // zone the listener is in (-1 none, -2 not decided yet)
    SpatialStats spatial;

    // A few mixed 3D voices per frame, in turn: a ray from the listener to the sound, stopping short of it
    void occlusion() {
        spatial.rays = spatial.occluded = 0;
        mixer.emitters(emitters);
        for (const mkaudio::Emitter& e : emitters) spatial.occluded += e.occlusion > 0.5f ? 1u : 0u;
        if (!world || emitters.empty()) return;
        const u32 n = std::min<u32>(kOcclusionRaysPerFrame, static_cast<u32>(emitters.size()));
        for (u32 i = 0; i < n; ++i) {
            const mkaudio::Emitter& e = emitters[(rayCursor + i) % emitters.size()];
            const Vec3 to = Vec3{e.position.x, e.position.y, e.position.z} - listener;
            const f32 distance = length(to);
            f32 amount = 0.0f;
            if (distance > kOcclusionNear * 2.0f) {
                physics::RayHit hit;
                amount = world->castRay(listener, to * ((distance - kOcclusionNear) / distance), hit) ? 1.0f : 0.0f;
                ++spatial.rays;
            }
            mixer.setOcclusion(e.handle, amount);
        }
        rayCursor = (rayCursor + n) % static_cast<u32>(emitters.size());
    }

    // The smallest zone containing the listener sets the reverb (none: back to dry)
    void reverbZone() {
        i32 best = -1;
        f32 bestVolume = 0.0f;
        for (size_t i = 0; i < zones.size(); ++i) {
            const ReverbZone& z = zones[i];
            const Quat inv{-z.rotation.x, -z.rotation.y, -z.rotation.z, z.rotation.w};
            const Vec3 p = rotate(inv, listener - z.center);
            if (std::fabs(p.x) > z.half.x || std::fabs(p.y) > z.half.y || std::fabs(p.z) > z.half.z) continue;
            const f32 volume = z.half.x * z.half.y * z.half.z;
            if (best < 0 || volume < bestVolume) {
                best = static_cast<i32>(i);
                bestVolume = volume;
            }
        }
        if (best == zone) return;
        const f32 fade = best >= 0 ? zones[static_cast<size_t>(best)].fade : zone >= 0 && zone < static_cast<i32>(zones.size()) ? zones[static_cast<size_t>(zone)].fade : 0.8f;
        mixer.setReverb(best >= 0 ? zones[static_cast<size_t>(best)].reverb : ReverbParams{}, fade);
        zone = best;
        spatial.zone = best >= 0 ? std::to_string(best) : std::string();
    }

    std::string findFile(bool& old) const {
        old = false;
        const std::string events = findAsset(std::string("assets/") + kEventsFile);
        if (!events.empty()) return events;
        const std::string table = findAsset(std::string("assets/") + kOldSoundTable);
        old = !table.empty();
        return table;
    }

    void loadEvents() {
        path = findFile(oldTable);
        fileTime = path.empty() ? 0 : fileWriteTime(path);
        file = {};
        std::vector<mkaudio::ParseMessage> messages;
        if (oldTable) {
            std::string text;
            readTextFile(path, text);
            mkaudio::parseEvents(tableToEvents(text), file, &messages);
            MEALKIT_WARN("Audio: %s is the old format - tools\\upgrade_project.ps1 converts it to assets/%s (MkAudio edits that)",
                         path.c_str(), kEventsFile);
        } else if (!path.empty()) {
            std::string text; // through readTextFile so it also comes from a pack (platform/Pack.h)
            readTextFile(path, text);
            mkaudio::parseEvents(text, file, &messages);
        }
        for (const mkaudio::ParseMessage& msg : messages)
            MEALKIT_WARN("%s:%u: %s%s", path.c_str(), msg.line, msg.error ? "" : "(warning) ", msg.text.c_str());
        mkaudio::resolveClips(file, [this](const std::string& name) {
            std::string found = findAssetFile(name);
            if (found.empty() && std::find(missingClips.begin(), missingClips.end(), name) == missingClips.end()) {
                MEALKIT_WARN("Audio: clip assets/%s not found", name.c_str());
                missingClips.push_back(name);
            }
            return found;
        });
        mixer.setBuses(file.buses);
        mixer.setEvents(file.events);
        mixer.setSnapshots(file.snapshots);
        mixer.setMusic(file.music);
        MEALKIT_INFO("Audio: %zu sound event(s), %zu music track(s), %zu snapshot(s) from %s", file.events.size(), file.music.size(),
                     file.snapshots.size(), path.empty() ? "(no sounds.events)" : path.c_str());
    }

    void pollFile() {
        if (time < fileCheck) return;
        fileCheck = time + kFileCheckSeconds;
        bool old = false;
        const std::string p = findFile(old);
        const unsigned long long t = p.empty() ? 0 : fileWriteTime(p);
        if (p != path || t != fileTime) loadEvents();
    }
};

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
    // With a pack mounted (shipping, platform/Pack.h) clips are read from it; otherwise miniaudio opens the loose files (and streams music)
    if (packsMounted())
        d.readFile = [](const std::string& file, std::vector<uint8_t>& out) {
            std::string bytes;
            if (!readTextFile(file, bytes)) return false;
            out.assign(bytes.begin(), bytes.end());
            return true;
        };
    if (!m->mixer.init(d)) return false;
    m->loadEvents();
    MEALKIT_INFO("Audio: %u Hz, %u channel(s), up to %u voices", m->mixer.sampleRate(), m->mixer.channels(), m->mixer.maxVoices());
    return true;
}

void Engine::shutdown() {
    m->mixer.shutdown();
    m->file = {};
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

void Engine::playMusic(std::string_view track) { m->mixer.playMusic(track); }

void Engine::playMusic(std::string_view track, Quantize at) { m->mixer.playMusic(track, at); }

void Engine::stopMusic(f32 fadeSeconds) { m->mixer.stopMusic(fadeSeconds); }

Handle Engine::stinger(std::string_view event, Quantize at, std::initializer_list<Param> params) { return m->mixer.stinger(event, at, params); }

MusicInfo Engine::music() const { return m->mixer.music(); }

void Engine::setSnapshot(std::string_view name, bool on) { m->mixer.setSnapshot(name, on); }

bool Engine::snapshot(std::string_view name) const { return m->mixer.snapshot(name); }

void Engine::setOcclusionWorld(const physics::PhysicsWorld* world) { m->world = world; }

void Engine::setReverbZones(const scene::Scene& scene) {
    std::vector<ReverbZone> zones;
    for (const scene::Entity& e : scene.entities) {
        if (!e.is(scene::kTypeReverbZone)) continue;
        const scene::Transform t = scene::worldTransform(scene, e);
        const scene::ReverbZoneFields f = scene::readObject<scene::ReverbZoneFields>(e);
        zones.push_back({t.position, t.rotation, t.scale * 0.5f, {f.roomSize, f.damping, f.wet, f.width}, f.fade});
    }
    setReverbZones(std::move(zones));
}

void Engine::setReverbZones(std::vector<ReverbZone> zones) {
    m->zones = std::move(zones);
    m->zone = -2; // old indices mean nothing now: decided again next update
}

const SpatialStats& Engine::spatialStats() const { return m->spatial; }

void Engine::resetMatch() {
    m->world = nullptr;
    setReverbZones(std::vector<ReverbZone>{});
    m->mixer.stopMusic(0.0f);
    std::vector<SnapshotInfo> snapshots;
    m->mixer.snapshots(snapshots);
    for (const SnapshotInfo& s : snapshots) m->mixer.setSnapshot(s.name, false);
}

void Engine::setListener(Vec3 position, Vec3 forward) {
    m->listener = position;
    m->mixer.setListener(toMk(position), toMk(forward));
}

Vec3 Engine::listener() const { return m->listener; }

void Engine::setBusVolume(std::string_view name, f32 volume) { m->mixer.setBusVolume(name, volume); }

f32 Engine::busVolume(std::string_view name) const { return m->mixer.busVolume(name); }

void Engine::setMuted(bool muted) { m->mixer.setMuted(muted); }

bool Engine::muted() const { return m->mixer.muted(); }

void Engine::update(f64 dt) {
    if (!m->mixer.ready()) return;
    MEALKIT_ZONE("Audio");
    m->time += dt;
    m->pollFile();
    m->reverbZone();
    m->mixer.update(dt);
    m->occlusion();
}

const std::vector<Subtitle>& Engine::subtitles() const { return m->mixer.subtitles(); }

void Engine::reloadEvents() {
    if (m->mixer.ready()) m->loadEvents();
}

const std::string& Engine::eventsPath() const { return m->path; }

std::vector<std::string> Engine::eventNames() const { return m->mixer.eventNames(); }

const EventDef* Engine::event(std::string_view name) const { return m->mixer.event(name); }

void Engine::voices(std::vector<VoiceInfo>& out) const { m->mixer.voices(out); }

const std::deque<CullInfo>& Engine::culls() const { return m->mixer.culls(); }

void Engine::buses(std::vector<BusInfo>& out) const { m->mixer.buses(out); }

void Engine::snapshots(std::vector<SnapshotInfo>& out) const { m->mixer.snapshots(out); }

const Stats& Engine::stats() const { return m->mixer.stats(); }

// ---------------------------------------------------------------- Live connection audio/

namespace {

// "a=b&c=d" (values decode %XX and +)
std::vector<std::pair<std::string, std::string>> formFields(const std::string& body) {
    auto decode = [](const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '+') out += ' ';
            else if (s[i] == '%' && i + 2 < s.size()) {
                out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
                i += 2;
            } else out += s[i];
        }
        return out;
    };
    std::vector<std::pair<std::string, std::string>> out;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('&', pos);
        if (end == std::string::npos) end = body.size();
        const std::string part = body.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = part.find('=');
        if (eq != std::string::npos) out.emplace_back(decode(part.substr(0, eq)), decode(part.substr(eq + 1)));
    }
    return out;
}

std::string number(f64 v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4g", v);
    return buf;
}

} // namespace

void addLiveService(live::Server& server, std::function<Engine*()> engineOf) {
    live::Service s;
    s.name = "audio";
    s.handle = [engineOf](const live::Request& req) {
        live::Response r;
        Engine* audio = engineOf ? engineOf() : nullptr;
        if (!audio || !audio->ready()) {
            r.status = 503;
            r.body = "{\"ok\":false,\"error\":\"no audio\"}";
            return r;
        }
        Engine& engine = *audio;
        if (req.method == "POST" && req.path == "play") {
            std::string event;
            std::vector<std::pair<std::string, f32>> params;
            for (const auto& [key, value] : formFields(req.body)) {
                if (key == "event") event = value;
                else if (key.rfind("p.", 0) == 0) params.emplace_back(key.substr(2), static_cast<f32>(std::strtod(value.c_str(), nullptr)));
            }
            if (!engine.event(event)) {
                r.status = 404;
                r.body = "{\"ok\":false,\"error\":" + live::jsonString("no event '" + event + "' (saved?)") + "}";
                return r;
            }
            const Handle h = engine.play(event, engine.listener()); // at the listener: 3D events are heard up close too
            for (const auto& [name, value] : params) engine.setParam(h, name, value);
            MEALKIT_INFO("Audio: live play %s", event.c_str());
            r.body = "{\"ok\":true}";
            return r;
        }
        if (req.method == "POST" && req.path == "reload") {
            engine.reloadEvents();
            r.body = "{\"ok\":true}";
            return r;
        }
        if (req.method == "GET" && req.path == "state") {
            const Stats& st = engine.stats();
            std::string out = "{\"file\":" + live::jsonString(engine.eventsPath()) + ",\"events\":" + std::to_string(engine.eventNames().size());
            out += ",\"stats\":{\"realVoices\":" + std::to_string(st.realVoices) + ",\"virtualVoices\":" + std::to_string(st.virtualVoices) +
                   ",\"updateMs\":" + number(st.updateMs) + "},\"voices\":[";
            std::vector<VoiceInfo> voices;
            engine.voices(voices);
            for (size_t i = 0; i < voices.size(); ++i)
                out += std::string(i ? "," : "") + "{\"event\":" + live::jsonString(voices[i].event) + ",\"virtual\":" +
                       (voices[i].virtualVoice ? "true" : "false") + ",\"loop\":" + (voices[i].loop ? "true" : "false") +
                       ",\"audibility\":" + number(voices[i].audibility) + ",\"distance\":" + number(voices[i].distance) +
                       ",\"occlusion\":" + number(voices[i].occlusion) + "}";
            out += "],\"buses\":[";
            std::vector<BusInfo> buses;
            engine.buses(buses);
            for (size_t i = 0; i < buses.size(); ++i)
                out += std::string(i ? "," : "") + "{\"name\":" + live::jsonString(buses[i].name) + ",\"volume\":" + number(buses[i].volume) +
                       ",\"mix\":" + number(buses[i].mix) + ",\"peak\":" + number(buses[i].peak) + ",\"duck\":" + number(buses[i].duck) +
                       ",\"snapshot\":" + number(buses[i].snapshot) + "}";
            const MusicInfo music = engine.music();
            out += "],\"music\":{\"track\":" + live::jsonString(music.track) + ",\"next\":" + live::jsonString(music.next) +
                   ",\"bar\":" + std::to_string(music.bar) + ",\"beat\":" + number(music.beat) + "},\"snapshots\":[";
            std::vector<SnapshotInfo> snapshots;
            engine.snapshots(snapshots);
            for (size_t i = 0; i < snapshots.size(); ++i)
                out += std::string(i ? "," : "") + "{\"name\":" + live::jsonString(snapshots[i].name) + ",\"on\":" + (snapshots[i].on ? "true" : "false") +
                       ",\"weight\":" + number(snapshots[i].weight) + "}";
            const SpatialStats& sp = engine.spatialStats();
            out += "],\"spatial\":{\"rays\":" + std::to_string(sp.rays) + ",\"occluded\":" + std::to_string(sp.occluded) +
                   ",\"zone\":" + live::jsonString(sp.zone) + "}";
            r.body = out + "}";
            return r;
        }
        r.status = 404;
        r.body = "{\"error\":\"not found\"}";
        return r;
    };
    server.addService(std::move(s));
}

} // namespace mealkit::audio
