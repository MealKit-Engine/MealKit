#include "mealkit/audio/Audio.h"

#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"
#include "mealkit/live/Server.h"
#include "mealkit/platform/Paths.h"

#include <mkaudio/Events.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// 접착층: 섞기·목소리 관리·이벤트 파일 형식은 런타임 mkaudio (third_party/mkaudio). 여기서는 파일을 찾아 넘기고, 에셋 경로·로그·프로파일·라이브 연결을 잇는다.
namespace mealkit::audio {
namespace {

constexpr f64 kFileCheckSeconds = 0.25;

mkaudio::Vec3 toMk(Vec3 v) { return {v.x, v.y, v.z}; }

// 0.5.1까지의 assets/sounds.table: 키 이름이 같아 "row <이름>"만 "event <이름>"으로 바꾸면 이벤트 형식이다
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
    mkaudio::EventFile file; // event()가 돌려주는 정의 (클립 파일은 찾은 경로)
    std::vector<std::string> missingClips; // 이미 경고한 클립 (다시 읽을 때 또 쓰지 않는다)
    f64 time = 0.0;
    f64 fileCheck = 0.0;
    std::string path; // 읽은 파일
    unsigned long long fileTime = 0;
    bool oldTable = false;
    Vec3 listener;

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
            MEALKIT_WARN("Audio: %s is the old format - tools\\upgrade_project.ps1 converts it to assets/%s (MealKit Audio edits that)",
                         path.c_str(), kEventsFile);
        } else if (!path.empty()) {
            mkaudio::loadEventFile(path, file, &messages);
        }
        for (const mkaudio::ParseMessage& msg : messages)
            MEALKIT_WARN("%s:%u: %s%s", path.c_str(), msg.line, msg.error ? "" : "(warning) ", msg.text.c_str());
        mkaudio::resolveClips(file, [this](const std::string& name) {
            std::string found = findAsset("assets/" + name);
            if (found.empty() && std::find(missingClips.begin(), missingClips.end(), name) == missingClips.end()) {
                MEALKIT_WARN("Audio: clip assets/%s not found", name.c_str());
                missingClips.push_back(name);
            }
            return found;
        });
        mixer.setBuses(file.buses);
        mixer.setEvents(file.events);
        MEALKIT_INFO("Audio: %zu sound event(s) from %s", file.events.size(), path.empty() ? "(no sounds.events)" : path.c_str());
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
    m->mixer.update(dt);
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

const Stats& Engine::stats() const { return m->mixer.stats(); }

// ---------------------------------------------------------------- 라이브 연결 audio/

namespace {

// "a=b&c=d" (값은 %XX와 + 풀기)
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
            const Handle h = engine.play(event, engine.listener()); // 리스너 자리: 3D 이벤트도 가까이에서 들린다
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
                       ",\"audibility\":" + number(voices[i].audibility) + ",\"distance\":" + number(voices[i].distance) + "}";
            out += "],\"buses\":[";
            std::vector<BusInfo> buses;
            engine.buses(buses);
            for (size_t i = 0; i < buses.size(); ++i)
                out += std::string(i ? "," : "") + "{\"name\":" + live::jsonString(buses[i].name) + ",\"volume\":" + number(buses[i].volume) +
                       ",\"mix\":" + number(buses[i].mix) + ",\"peak\":" + number(buses[i].peak) + "}";
            r.body = out + "]}";
            return r;
        }
        r.status = 404;
        r.body = "{\"error\":\"not found\"}";
        return r;
    };
    server.addService(std::move(s));
}

} // namespace mealkit::audio
