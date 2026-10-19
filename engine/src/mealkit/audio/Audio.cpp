#include "mealkit/audio/Audio.h"

#include "mealkit/audio/MiniaudioConfig.h"
#include "mealkit/core/Clock.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"
#include "mealkit/core/Random.h"
#include "mealkit/data/Tables.h"
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

namespace mealkit::audio {
namespace {

constexpr u32 kVoiceSlots = 256;       // 실제 + 가상 + 시작 대기
constexpr u32 kMaxLayers = 4;          // 블렌드 컨테이너가 같이 내는 클립 수
constexpr size_t kCullHistory = 64;    // [소리] 패널의 끊긴 소리 목록
constexpr f32 kInaudible = 0.002f;     // 이보다 작으면 들리지 않는 것으로 본다 (가상 목소리)
constexpr f64 kTableCheckSeconds = 0.25;

enum class Container : u8 { Random, Switch, Blend };
enum class Steal : u8 { Oldest, Quietest, None };

// 버스 출력의 최댓값을 재는 노드 (오디오 스레드가 적고 메인 스레드가 읽는다)
struct MeterNode {
    ma_node_base base;
    std::atomic<f32> peak{0.0f};
};

void meterProcess(ma_node* node, const float** framesIn, ma_uint32* frameCountIn, float** framesOut,
                  ma_uint32* frameCountOut) {
    MeterNode* meter = reinterpret_cast<MeterNode*>(node);
    const ma_uint32 channels = ma_node_get_output_channels(node, 0);
    if (!framesIn || !framesIn[0]) { // 아무것도 연결되지 않았다
        std::fill(framesOut[0], framesOut[0] + *frameCountOut * channels, 0.0f);
        return;
    }
    const ma_uint32 frames = std::min(*frameCountIn, *frameCountOut);
    f32 peak = meter->peak.load(std::memory_order_relaxed) * 0.97f; // 천천히 떨어진다
    const ma_uint32 samples = frames * channels;
    for (ma_uint32 i = 0; i < samples; ++i) {
        const f32 s = framesIn[0][i];
        framesOut[0][i] = s;
        peak = std::max(peak, std::fabs(s));
    }
    *frameCountIn = frames;
    *frameCountOut = frames;
    meter->peak.store(peak, std::memory_order_relaxed);
}

ma_node_vtable g_meterVtable = {meterProcess, nullptr, 1, 1, 0};

struct Bus {
    std::string name;
    ma_sound_group group;
    MeterNode meter;
    f32 volume = 1.0f;
};

struct Clip {
    std::string path; // assets 기준
    std::string file; // 찾은 파일 (없으면 빈 문자열)
    bool decoded = false;
    ma_sound source; // 미리 풀어 둔 원본 (복사해서 재생한다, decoded일 때만)
};

struct EventInfo {
    std::string name;
    SoundEvent def;
    std::vector<u32> clips; // m->clips 번호
    Container container = Container::Random;
    Steal steal = Steal::Oldest;
    u32 bus = 0;
    f64 lastPlay = -1e9;
    u32 lastClip = 0xFFFFFFFFu;
};

struct Layer {
    ma_sound sound;
    ma_lpf_node lpf;
    bool sound_ = false;
    bool lpf_ = false;
    u32 clip = 0;
    f32 cutoff = 0.0f; // 지금 걸린 로우패스 (같으면 다시 만들지 않는다)
};

struct Voice {
    u32 generation = 1;
    bool used = false;
    bool pending = true; // play() 다음 update()를 기다림
    bool real = false;   // 믹싱 중
    std::string eventName;
    u32 event = 0;
    bool spatial = false;
    Vec3 position;
    std::vector<std::pair<std::string, f32>> params;
    f32 strength = 0.0f; // 같은 프레임 묶기의 세기 (곡선 파라미터, 없으면 볼륨)
    f32 volumeScale = 1.0f, pitchScale = 1.0f;
    u32 clip = 0; // 무작위·스위치가 고른 클립 (블렌드는 전부)
    Layer layers[kMaxLayers];
    u32 layerCount = 0;
    f64 startTime = 0.0;
    f64 virtualCursor = 0.0; // 가상 목소리가 센 재생 위치 (초)
    f64 fadeStart = 0.0, fadeEnd = 0.0;
    bool fading = false;
    f32 audibility = 0.0f;
    f32 distance = 0.0f;
};

f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

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
    ma_engine engine;
    bool ready = false;
    bool muted = false;
    EngineDesc desc;
    std::vector<std::unique_ptr<Bus>> buses; // [0] = master
    std::vector<std::unique_ptr<Clip>> clips;
    std::vector<EventInfo> events;
    std::unique_ptr<Voice[]> voices;
    std::vector<std::pair<std::string, f32>> globals;
    std::deque<CullInfo> culls;
    Stats stats;
    Pcg32 rng{0x5EEDu};
    f64 time = 0.0;
    std::vector<Subtitle> subtitles;
    f64 tableCheck = 0.0;
    std::string tablePath;
    unsigned long long tableTime = 0;
    Vec3 listener;

    // ---------------------------------------------------------------- 버스
    u32 bus(std::string_view name) {
        for (u32 i = 0; i < buses.size(); ++i)
            if (buses[i]->name == name) return i;
        auto b = std::make_unique<Bus>();
        b->name = std::string(name);
        // 버스 → 미터 → (master 버스 → master 미터) → 장치
        ma_sound_group* parent = buses.empty() ? nullptr : &buses[0]->group;
        if (ma_sound_group_init(&engine, 0, parent, &b->group) != MA_SUCCESS) {
            MEALKIT_WARN("Audio: cannot create bus %s", b->name.c_str());
            return 0;
        }
        const ma_uint32 channels = ma_engine_get_channels(&engine);
        ma_node_config config = ma_node_config_init();
        config.vtable = &g_meterVtable;
        config.pInputChannels = &channels;
        config.pOutputChannels = &channels;
        if (ma_node_init(ma_engine_get_node_graph(&engine), &config, nullptr, &b->meter.base) == MA_SUCCESS) {
            ma_node* next = parent ? static_cast<ma_node*>(parent) : ma_engine_get_endpoint(&engine);
            ma_node_attach_output_bus(&b->group, 0, &b->meter, 0);
            ma_node_attach_output_bus(&b->meter, 0, next, 0);
        }
        buses.push_back(std::move(b));
        return static_cast<u32>(buses.size() - 1);
    }

    void cull(const std::string& event, const char* reason) {
        culls.push_back({event, reason, time});
        if (culls.size() > kCullHistory) culls.pop_front();
        ++stats.culledThisFrame;
    }

    // ---------------------------------------------------------------- 표
    u32 clip(const std::string& path, bool stream) {
        for (u32 i = 0; i < clips.size(); ++i)
            if (clips[i]->path == path) {
                if (!stream && !clips[i]->decoded) decode(*clips[i]);
                return i;
            }
        auto c = std::make_unique<Clip>();
        c->path = path;
        c->file = findAsset("assets/" + path);
        if (c->file.empty()) MEALKIT_WARN("Audio: clip assets/%s not found", path.c_str());
        else if (!stream) decode(*c);
        clips.push_back(std::move(c));
        return static_cast<u32>(clips.size() - 1);
    }

    // 효과음은 매치를 불러올 때 메모리에 풀어 둔다 (재생 때 디스크를 읽지 않는다)
    void decode(Clip& c) {
        if (c.file.empty() || c.decoded) return;
        if (ma_sound_init_from_file(&engine, c.file.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                    nullptr, nullptr, &c.source) != MA_SUCCESS) {
            MEALKIT_WARN("Audio: cannot decode %s", c.file.c_str());
            return;
        }
        c.decoded = true;
    }

    void loadEvents() {
        tablePath = findAsset(std::string("assets/") + kSoundTable);
        tableTime = tablePath.empty() ? 0 : fileWriteTime(tablePath);
        sim::Table<SoundEvent> table;
        if (!tablePath.empty()) data::loadTable(table, kSoundTable);
        std::vector<EventInfo> next;
        for (u32 i = 0; i < table.size(); ++i) {
            EventInfo e;
            e.name = table.ids[i];
            e.def = table.rows[i];
            for (const std::string& path : splitClips(e.def.clips)) e.clips.push_back(clip(path, e.def.stream));
            const std::string& c = e.def.container;
            e.container = c == "blend" ? Container::Blend : c == "switch" ? Container::Switch : Container::Random;
            if (c != "random" && c != "blend" && c != "switch")
                MEALKIT_WARN("Audio: event %s: unknown container '%s' (random, switch, blend)", e.name.c_str(), c.c_str());
            e.steal = e.def.steal == "quietest" ? Steal::Quietest : e.def.steal == "none" ? Steal::None : Steal::Oldest;
            e.bus = bus(e.def.bus.empty() ? "sfx" : e.def.bus);
            for (const EventInfo& old : events) // 쿨다운·직전 클립은 다시 읽어도 이어 간다
                if (old.name == e.name) {
                    e.lastPlay = old.lastPlay;
                    e.lastClip = old.lastClip;
                }
            next.push_back(std::move(e));
        }
        events = std::move(next);
        // 재생 중인 소리는 이름으로 새 행을 찾는다 (없어진 이벤트는 멈춘다)
        for (u32 i = 0; i < kVoiceSlots; ++i) {
            Voice& v = voices[i];
            if (!v.used) continue;
            const EventInfo* e = find(v.eventName);
            if (e) v.event = static_cast<u32>(e - events.data());
            else release(v);
        }
        MEALKIT_INFO("Audio: %zu sound event(s) from %s", events.size(), tablePath.empty() ? "(no sounds.table)" : tablePath.c_str());
    }

    const EventInfo* find(std::string_view name) const {
        for (const EventInfo& e : events)
            if (e.name == name) return &e;
        return nullptr;
    }

    // ---------------------------------------------------------------- 목소리
    f32 param(const Voice& v, const std::string& name, f32 fallback) const {
        for (const auto& [n, value] : v.params)
            if (n == name) return value;
        for (const auto& [n, value] : globals)
            if (n == name) return value;
        return fallback;
    }

    // 곡선 파라미터의 0..1 위치
    f32 curveT(const Voice& v, const SoundEvent& d) const {
        if (d.param.empty()) return 0.0f;
        const f32 p = param(v, d.param, d.paramMin);
        const f32 range = d.paramMax - d.paramMin;
        return range != 0.0f ? std::clamp((p - d.paramMin) / range, 0.0f, 1.0f) : 0.0f;
    }

    void unrealize(Voice& v) {
        for (u32 i = 0; i < v.layerCount; ++i) {
            Layer& l = v.layers[i];
            if (l.sound_) ma_sound_uninit(&l.sound);
            if (l.lpf_) ma_lpf_node_uninit(&l.lpf, nullptr);
            l.sound_ = l.lpf_ = false;
        }
        v.real = false;
    }

    void release(Voice& v) {
        unrealize(v);
        v.used = false;
        v.params.clear();
        ++v.generation;
    }

    bool realize(Voice& v, f64 cursor) {
        const EventInfo& e = events[v.event];
        const SoundEvent& d = e.def;
        // 블렌드는 모든 클립을 같이, 나머지는 고른 클립 하나
        std::vector<u32> chosen;
        if (e.container == Container::Blend)
            for (u32 i = 0; i < e.clips.size() && i < kMaxLayers; ++i) chosen.push_back(e.clips[i]);
        else if (!e.clips.empty())
            chosen.push_back(e.clips[v.clip % e.clips.size()]);
        v.layerCount = 0;
        for (const u32 ci : chosen) {
            Clip& c = *clips[ci];
            if (c.file.empty()) continue;
            Layer& l = v.layers[v.layerCount];
            l.clip = ci;
            const bool lowpass = d.lowpassAtMin > 0.0f || d.lowpassAtMax > 0.0f;
            ma_node* target = &buses[e.bus]->group;
            if (lowpass) {
                const ma_uint32 channels = ma_engine_get_channels(&engine);
                ma_lpf_node_config lc = ma_lpf_node_config_init(channels, ma_engine_get_sample_rate(&engine),
                                                                std::max(d.lowpassAtMin, 20.0f), 2);
                if (ma_lpf_node_init(ma_engine_get_node_graph(&engine), &lc, nullptr, &l.lpf) == MA_SUCCESS) {
                    l.lpf_ = true;
                    l.cutoff = std::max(d.lowpassAtMin, 20.0f);
                    ma_node_attach_output_bus(&l.lpf, 0, target, 0);
                }
            }
            ma_result r;
            const ma_uint32 flags = d.spatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
            if (c.decoded && !d.stream)
                r = ma_sound_init_copy(&engine, &c.source, flags, nullptr, &l.sound);
            else
                r = ma_sound_init_from_file(&engine, c.file.c_str(), flags | MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                            &l.sound);
            if (r != MA_SUCCESS) {
                if (l.lpf_) ma_lpf_node_uninit(&l.lpf, nullptr);
                l.lpf_ = false;
                continue;
            }
            l.sound_ = true;
            ma_node_attach_output_bus(&l.sound, 0, l.lpf_ ? static_cast<ma_node*>(&l.lpf) : target, 0);
            ma_sound_set_looping(&l.sound, d.loop);
            if (d.spatial) {
                ma_sound_set_attenuation_model(&l.sound, ma_attenuation_model_linear);
                ma_sound_set_min_distance(&l.sound, d.minDistance);
                ma_sound_set_max_distance(&l.sound, std::max(d.maxDistance, d.minDistance + 0.01f));
            }
            if (cursor > 0.0) { // 가상에서 돌아온 루프: 센 위치에서 잇는다
                ma_uint64 length = 0;
                ma_sound_get_length_in_pcm_frames(&l.sound, &length);
                const ma_uint32 rate = ma_engine_get_sample_rate(&engine);
                if (length > 0) ma_sound_seek_to_pcm_frame(&l.sound, static_cast<ma_uint64>(cursor * rate) % length);
            }
            ++v.layerCount;
        }
        if (v.layerCount == 0) return false;
        v.real = true;
        apply(v);
        for (u32 i = 0; i < v.layerCount; ++i) ma_sound_start(&v.layers[i].sound);
        return true;
    }

    // 파라미터·위치·페이드를 실제 목소리에 넣는다
    void apply(Voice& v) {
        const EventInfo& e = events[v.event];
        const SoundEvent& d = e.def;
        const f32 t = curveT(v, d);
        f32 volume = d.volume * v.volumeScale * lerpf(d.volumeAtMin, d.volumeAtMax, t);
        if (v.fading) volume *= static_cast<f32>(std::clamp((v.fadeEnd - time) / std::max(v.fadeEnd - v.fadeStart, 1e-4), 0.0, 1.0));
        const f32 pitch = std::max(0.01f, d.pitch * v.pitchScale * lerpf(d.pitchAtMin, d.pitchAtMax, t));
        const f32 cutoff = lerpf(d.lowpassAtMin, d.lowpassAtMax, t);
        for (u32 i = 0; i < v.layerCount; ++i) {
            Layer& l = v.layers[i];
            f32 weight = 1.0f;
            if (e.container == Container::Blend && v.layerCount > 1) { // 이웃한 두 클립을 크로스페이드
                const f32 at = (d.param.empty() ? 0.0f : t) * static_cast<f32>(v.layerCount - 1);
                weight = std::max(0.0f, 1.0f - std::fabs(at - static_cast<f32>(i)));
            }
            ma_sound_set_volume(&l.sound, volume * weight);
            ma_sound_set_pitch(&l.sound, pitch);
            if (d.spatial) ma_sound_set_position(&l.sound, v.position.x, v.position.y, -v.position.z);
            if (l.lpf_ && cutoff > 0.0f && std::fabs(cutoff - l.cutoff) > 1.0f) {
                l.cutoff = cutoff;
                ma_lpf_config lc = ma_lpf_config_init(ma_format_f32, ma_engine_get_channels(&engine),
                                                      ma_engine_get_sample_rate(&engine), std::max(cutoff, 20.0f), 2);
                ma_lpf_node_reinit(&lc, &l.lpf);
            }
        }
    }

    f32 estimate(Voice& v) const { // 볼륨 × 거리 감쇠 (가상 목소리·상한 판단)
        const SoundEvent& d = events[v.event].def;
        const f32 t = curveT(v, d);
        f32 gain = d.volume * v.volumeScale * lerpf(d.volumeAtMin, d.volumeAtMax, t);
        v.distance = 0.0f;
        if (d.spatial) {
            v.distance = length(v.position - listener);
            const f32 range = std::max(d.maxDistance - d.minDistance, 0.01f);
            gain *= std::clamp(1.0f - (v.distance - d.minDistance) / range, 0.0f, 1.0f);
        }
        return gain;
    }

    bool finished(const Voice& v) const {
        if (!v.real) return false;
        for (u32 i = 0; i < v.layerCount; ++i)
            if (!ma_sound_at_end(&v.layers[i].sound)) return false;
        return true;
    }

    void start(Voice& v) {
        EventInfo& e = events[v.event];
        const SoundEvent& d = e.def;
        v.pending = false;
        v.startTime = time;
        v.volumeScale = 1.0f + (d.volumeRandom > 0.0f ? rng.range(-d.volumeRandom, d.volumeRandom) : 0.0f);
        v.pitchScale = 1.0f + (d.pitchRandom > 0.0f ? rng.range(-d.pitchRandom, d.pitchRandom) : 0.0f);
        const u32 n = static_cast<u32>(e.clips.size());
        if (e.container == Container::Switch && n > 0) {
            const f32 p = param(v, d.param, 0.0f);
            v.clip = static_cast<u32>(std::clamp(std::lround(p), 0l, static_cast<long>(n) - 1));
        } else if (n > 1) { // 무작위: 직전 클립은 다시 고르지 않는다
            v.clip = rng.below(n - 1);
            if (v.clip >= e.lastClip && e.lastClip < n) ++v.clip;
        } else {
            v.clip = 0;
        }
        e.lastClip = v.clip;
        e.lastPlay = time;
        ++stats.playsThisFrame;
    }

    Handle handleOf(u32 index) const { return {index, voices[index].generation}; }
    Voice* voiceOf(Handle h) const {
        if (!voices || !h.valid() || h.index >= kVoiceSlots) return nullptr;
        Voice& v = voices[h.index];
        return v.used && v.generation == h.generation ? &v : nullptr;
    }

    Handle request(std::string_view name, bool spatial, Vec3 position, std::initializer_list<Param> params) {
        if (!ready) return {};
        const EventInfo* e = find(name);
        if (!e) {
            cull(std::string(name), "missing event");
            return {};
        }
        if (!e->def.subtitle.empty()) { // 자막: 같은 키는 한 줄로
            auto at = std::find_if(subtitles.begin(), subtitles.end(), [&](const Subtitle& s) { return s.key == e->def.subtitle; });
            if (at != subtitles.end()) at->secondsLeft = std::max(at->secondsLeft, e->def.subtitleSeconds);
            else subtitles.push_back({e->def.subtitle, e->def.subtitleSeconds});
        }
        for (u32 i = 0; i < kVoiceSlots; ++i) {
            Voice& v = voices[i];
            if (v.used) continue;
            v.used = true;
            v.pending = true;
            v.real = v.fading = false;
            v.eventName = e->name;
            v.event = static_cast<u32>(e - events.data());
            v.spatial = spatial && e->def.spatial;
            v.position = position;
            v.params.clear();
            for (const Param& p : params) v.params.emplace_back(p.name, p.value);
            v.strength = e->def.param.empty() ? e->def.volume : param(v, e->def.param, e->def.paramMin);
            v.layerCount = 0;
            v.virtualCursor = 0.0;
            return handleOf(i);
        }
        cull(e->name, "voice limit");
        return {};
    }

    // 시작 대기 중인 요청: 이벤트마다 쿨다운, 동시 재생 상한(넘치면 끊을 것), 같은 프레임 묶기(세기 큰 순)
    void startPending() {
        std::vector<u32> pending;
        for (u32 i = 0; i < kVoiceSlots; ++i)
            if (voices[i].used && voices[i].pending) pending.push_back(i);
        std::sort(pending.begin(), pending.end(), [&](u32 a, u32 b) {
            const Voice &va = voices[a], &vb = voices[b];
            if (va.event != vb.event) return va.event < vb.event;
            if (va.strength != vb.strength) return va.strength > vb.strength;
            return a < b;
        });
        for (size_t at = 0; at < pending.size();) {
            const u32 event = voices[pending[at]].event;
            size_t end = at;
            while (end < pending.size() && voices[pending[end]].event == event) ++end;
            EventInfo& e = events[event];
            const SoundEvent& d = e.def;
            const bool cooling = d.cooldown > 0.0f && time - e.lastPlay < d.cooldown;
            u32 active = 0;
            for (u32 i = 0; i < kVoiceSlots; ++i)
                if (voices[i].used && !voices[i].pending && voices[i].event == event) ++active;
            const u32 cap = std::max(d.maxInstances, 1u);
            for (size_t k = at; k < end; ++k) {
                Voice& v = voices[pending[k]];
                if (cooling) {
                    cull(e.name, "cooldown");
                    release(v);
                    continue;
                }
                if (k - at >= cap) { // 한 프레임에 상한보다 많이 왔다: 세기 큰 것만
                    cull(e.name, "same frame");
                    release(v);
                    continue;
                }
                if (active >= cap) {
                    Voice* victim = nullptr;
                    if (e.steal != Steal::None)
                        for (u32 i = 0; i < kVoiceSlots; ++i) {
                            Voice& o = voices[i];
                            if (!o.used || o.pending || o.event != event) continue;
                            if (!victim || (e.steal == Steal::Oldest ? o.startTime < victim->startTime
                                                                     : o.audibility < victim->audibility))
                                victim = &o;
                        }
                    if (!victim) {
                        cull(e.name, "max instances");
                        release(v);
                        continue;
                    }
                    cull(e.name, "max instances (stolen)");
                    release(*victim);
                    --active;
                }
                start(v);
                ++active;
            }
            at = end;
        }
    }

    // 실제로 믹싱할 목소리 고르기: 우선순위 × 들리는 크기. 나머지 루프는 가상(재생 위치만 센다), 짧은 소리는 끊는다
    void manageVoices(f64 dt) {
        std::vector<u32> active;
        for (u32 i = 0; i < kVoiceSlots; ++i) {
            Voice& v = voices[i];
            if (!v.used || v.pending) continue;
            if (finished(v) || (v.fading && time >= v.fadeEnd)) {
                release(v);
                continue;
            }
            v.audibility = estimate(v);
            if (!v.real) v.virtualCursor += dt * events[v.event].def.pitch;
            active.push_back(i);
        }
        std::sort(active.begin(), active.end(), [&](u32 a, u32 b) {
            const Voice &va = voices[a], &vb = voices[b];
            const f32 sa = static_cast<f32>(events[va.event].def.priority + 1) * va.audibility;
            const f32 sb = static_cast<f32>(events[vb.event].def.priority + 1) * vb.audibility;
            if (sa != sb) return sa > sb;
            return va.startTime > vb.startTime;
        });
        u32 real = 0;
        stats.realVoices = stats.virtualVoices = 0;
        for (const u32 i : active) {
            Voice& v = voices[i];
            const bool loop = events[v.event].def.loop;
            const bool audible = v.audibility >= kInaudible && real < desc.maxVoices;
            if (audible) {
                if (!v.real) {
                    // 처음 시작이거나 가상에서 돌아온다 (짧은 소리는 처음 시작일 때만 여기로 온다)
                    if (!realize(v, loop ? v.virtualCursor : 0.0)) {
                        cull(v.eventName, "missing clip");
                        release(v);
                        continue;
                    }
                } else {
                    apply(v);
                }
                ++real;
                ++stats.realVoices;
                continue;
            }
            if (!loop) { // 짧은 소리는 가상으로 두지 않는다
                cull(v.eventName, v.audibility < kInaudible ? "inaudible" : "voice limit");
                release(v);
                continue;
            }
            if (v.real) {
                ma_uint64 cursor = 0;
                ma_sound_get_cursor_in_pcm_frames(&v.layers[0].sound, &cursor);
                v.virtualCursor = static_cast<f64>(cursor) / ma_engine_get_sample_rate(&engine);
                unrealize(v);
            }
            ++stats.virtualVoices;
        }
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
    m->desc = desc;
    m->desc.maxVoices = std::clamp(desc.maxVoices, 1u, kVoiceSlots);
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;
    config.channels = 2; // 스테레오로 섞고 장치 채널(7.1 등)로는 miniaudio가 바꾼다 (섞는 비용이 채널 수에 비례)
    if (ma_engine_init(&config, &m->engine) != MA_SUCCESS) {
        MEALKIT_WARN("Audio: no output device - running without sound");
        return false;
    }
    m->ready = true;
    m->voices = std::make_unique<Voice[]>(kVoiceSlots);
    m->bus("master");
    for (const char* name : {"music", "sfx", "ui", "voice"}) m->bus(name);
    m->rng = Pcg32(static_cast<u64>(nowSeconds() * 1000.0) | 1u); // 화면 쪽 난수 (결정론과 무관)
    m->loadEvents();
    setMuted(desc.muted);
    MEALKIT_INFO("Audio: %u Hz, %u channel(s), up to %u voices", ma_engine_get_sample_rate(&m->engine),
                 ma_engine_get_channels(&m->engine), m->desc.maxVoices);
    return true;
}

void Engine::shutdown() {
    if (!m->ready) return;
    for (u32 i = 0; i < kVoiceSlots; ++i)
        if (m->voices[i].used) m->release(m->voices[i]);
    for (auto& c : m->clips)
        if (c->decoded) ma_sound_uninit(&c->source);
    m->clips.clear();
    for (size_t i = m->buses.size(); i-- > 0;) { // 자식 버스부터
        ma_node_uninit(&m->buses[i]->meter.base, nullptr);
        ma_sound_group_uninit(&m->buses[i]->group);
    }
    m->buses.clear();
    m->events.clear();
    ma_engine_uninit(&m->engine);
    m->voices.reset();
    m->ready = false;
}

bool Engine::ready() const { return m->ready; }

Handle Engine::play(std::string_view event, Vec3 position, std::initializer_list<Param> params) {
    return m->request(event, true, position, params);
}

Handle Engine::play2D(std::string_view event, std::initializer_list<Param> params) {
    return m->request(event, false, {}, params);
}

void Engine::setParam(Handle h, std::string_view name, f32 value) {
    Voice* v = m->voiceOf(h);
    if (!v) return;
    for (auto& [n, val] : v->params)
        if (n == name) {
            val = value;
            return;
        }
    v->params.emplace_back(std::string(name), value);
}

void Engine::setPosition(Handle h, Vec3 position) {
    if (Voice* v = m->voiceOf(h)) v->position = position;
}

void Engine::stop(Handle h, f32 fadeSeconds) {
    Voice* v = m->voiceOf(h);
    if (!v) return;
    if (fadeSeconds <= 0.0f || !v->real) {
        m->release(*v);
        return;
    }
    v->fading = true;
    v->fadeStart = m->time;
    v->fadeEnd = m->time + fadeSeconds;
}

void Engine::stopAll() {
    if (!m->ready) return;
    for (u32 i = 0; i < kVoiceSlots; ++i)
        if (m->voices[i].used) m->release(m->voices[i]);
}

bool Engine::playing(Handle h) const { return m->voiceOf(h) != nullptr; }

void Engine::setGlobalParam(std::string_view name, f32 value) {
    for (auto& [n, v] : m->globals)
        if (n == name) {
            v = value;
            return;
        }
    m->globals.emplace_back(std::string(name), value);
}

void Engine::setListener(Vec3 position, Vec3 forward) {
    if (!m->ready) return;
    m->listener = position;
    ma_engine_listener_set_position(&m->engine, 0, position.x, position.y, -position.z); // 왼손 좌표 → miniaudio 오른손
    ma_engine_listener_set_direction(&m->engine, 0, forward.x, forward.y, -forward.z);
    ma_engine_listener_set_world_up(&m->engine, 0, 0.0f, 1.0f, 0.0f);
}

void Engine::setBusVolume(std::string_view name, f32 volume) {
    if (!m->ready) return;
    Bus& b = *m->buses[m->bus(name)];
    b.volume = std::max(volume, 0.0f);
    ma_sound_group_set_volume(&b.group, b.volume);
}

f32 Engine::busVolume(std::string_view name) const {
    for (const auto& b : m->buses)
        if (b->name == name) return b->volume;
    return 1.0f;
}

void Engine::setMuted(bool muted) {
    m->muted = muted;
    if (m->ready) ma_engine_set_volume(&m->engine, muted ? 0.0f : 1.0f);
}

bool Engine::muted() const { return m->muted; }

void Engine::update(f64 dt) {
    if (!m->ready) return;
    MEALKIT_ZONE("Audio");
    const f64 begin = nowSeconds();
    m->time += dt;
    for (Subtitle& s : m->subtitles) s.secondsLeft -= static_cast<f32>(dt);
    std::erase_if(m->subtitles, [](const Subtitle& s) { return s.secondsLeft <= 0.0f; });
    m->stats.playsThisFrame = m->stats.culledThisFrame = 0;
    m->pollTable();
    m->startPending();
    m->manageVoices(dt);
    m->stats.updateMs = (nowSeconds() - begin) * 1000.0;
}

const std::vector<Subtitle>& Engine::subtitles() const { return m->subtitles; }

void Engine::reloadEvents() {
    if (m->ready) m->loadEvents();
}

std::vector<std::string> Engine::eventNames() const {
    std::vector<std::string> names;
    for (const EventInfo& e : m->events) names.push_back(e.name);
    return names;
}

const SoundEvent* Engine::event(std::string_view name) const {
    const EventInfo* e = m->find(name);
    return e ? &e->def : nullptr;
}

void Engine::voices(std::vector<VoiceInfo>& out) const {
    out.clear();
    if (!m->ready) return;
    for (u32 i = 0; i < kVoiceSlots; ++i) {
        const Voice& v = m->voices[i];
        if (!v.used || v.pending) continue;
        out.push_back({v.eventName, !v.real, m->events[v.event].def.loop, v.audibility, v.distance, m->time - v.startTime});
    }
}

const std::deque<CullInfo>& Engine::culls() const { return m->culls; }

void Engine::buses(std::vector<BusInfo>& out) const {
    out.clear();
    for (const auto& b : m->buses) out.push_back({b->name, b->volume, b->meter.peak.load(std::memory_order_relaxed)});
}

const Stats& Engine::stats() const { return m->stats; }

} // namespace mealkit::audio
