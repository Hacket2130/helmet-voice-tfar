#pragma once

#include "dsp/DeathTrooperEffect.hpp"
#include "dsp/HelmetVoiceEffect.hpp"
#include <charconv>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// No TeamSpeak/Win32 dependencies here: the exported early playback callback
// and the native harness exercise this exact state machine and approved DSP.
namespace tfv {
using Millis = std::uint64_t;
constexpr Millis LocalTTL = 3000, RemoteTTL = 6000, ContextTTL = 1500, AudioGap = 250;
constexpr std::size_t MaxRecord = 4095, MaxPeers = 1024;
inline Millis nowMillis() noexcept {
    return static_cast<Millis>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline bool fresh(Millis now, Millis then, Millis ttl) noexcept { return now >= then && now - then < ttl; }
inline bool modeValid(int mode) noexcept { return mode == -1 || mode == 0 || mode == 1 || mode == 3 || mode == 4; }
inline bool safeToken(std::string_view s, std::size_t limit, bool base64 = false) noexcept {
    if (s.empty() || s.size() > limit) return false;
    for (char c : s) {
        const bool alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!alnum && (base64 ? c != '+' && c != '/' && c != '=' : c != '_' && c != '-')) return false;
    }
    return true;
}
inline std::vector<std::string_view> split(std::string_view s, char separator) {
    std::vector<std::string_view> result;
    std::size_t start = 0;
    for (;;) {
        auto end = s.find(separator, start);
        result.push_back(s.substr(start, end == s.npos ? s.size() - start : end - start));
        if (end == s.npos) return result;
        start = end + 1;
    }
}
template<class T> inline bool integer(std::string_view s, T& output) noexcept {
    if (s.empty()) return false;
    auto parsed = std::from_chars(s.data(), s.data() + s.size(), output);
    return parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size();
}
struct Settings {
    std::array<float, 23> values{{.25f,.35f,150,-6,32,.75f,.25f,.65f,8,.12f,1,-48,1,3200,.22f,
                                623,3100,2.4f,1,2000,3,2500,2}};
    bool operator==(const Settings& other) const noexcept { return values == other.values; }
};
inline bool parseSettings(std::string_view s, Settings& output) {
    constexpr std::array<float, 23> minimum{{0,.05f,80,-12,8,0,0,0,8,0,0,-70,0,1800,0,80,1200,1,0,500,0,1000,0}};
    constexpr std::array<float, 23> maximum{{.6f,1.5f,1200,0,48,1,1,1,16,1,1,-20,10,12000,.6f,1200,10000,6,1,4000,9,4000,6}};
    auto fields = split(s, ',');
    if (fields.size() != output.values.size()) return false;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        auto field = fields[i];
        if (field.empty() || field.size() > 32) return false;
        float value = 0;
        auto parsed = std::from_chars(field.data(), field.data() + field.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() || !std::isfinite(value) ||
            value < minimum[i] || value > maximum[i]) return false;
        if (i == 10 && value != 0 && value != 1) return false;
        if (i == 8 && value != 8 && value != 12 && value != 16) return false;
        output.values[i] = value;
    }
    return true;
}
struct LocalState {
    std::string serverUID, mission;
    int mode = -1;
    Settings settings;
    Millis received = 0;
};
inline bool parseLocal(std::string_view raw, LocalState& output) {
    if (raw.size() > MaxRecord || raw.find('\0') != raw.npos) return false;
    auto fields = split(raw, '\t');
    if (fields.size() != 5 || fields[0] != "TFV12" || !safeToken(fields[1], 128, true) ||
        !safeToken(fields[2], 64) || !integer(fields[3], output.mode) || !modeValid(output.mode) ||
        !parseSettings(fields[4], output.settings)) return false;
    output.serverUID = fields[1]; output.mission = fields[2];
    return true;
}
struct PeerMessage {
    std::string mission;
    int mode = -1;
    std::uint64_t sequence = 0;
};
inline bool parsePeer(std::string_view raw, PeerMessage& output) {
    if (raw.size() > 128 || raw.find('\0') != raw.npos) return false;
    auto fields = split(raw, '\t');
    if (fields.size() != 4 || fields[0] != "TFV12" || !safeToken(fields[1], 64) ||
        !integer(fields[2], output.mode) || !modeValid(output.mode) ||
        !integer(fields[3], output.sequence) || output.sequence == 0) return false;
    output.mission = fields[1]; return true;
}
inline bool ownPlugin(std::string_view name) {
    if (name.size() > 64) return false;
    std::string lower(name);
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    // TeamSpeak supplies the plugin binary's name, not an invoker-selected ID.
    return lower == "task_force_vanguard_win64" || lower == "task_force_vanguard" ||
           lower == "task force vanguard tfar addon";
}
inline bool activeTFARMetadata(std::string_view metadata) noexcept {
    auto start = metadata.find("<TFAR>");
    if (start == metadata.npos) return false;
    auto end = metadata.find("</TFAR>", start);
    if (end == metadata.npos) return false;
    auto block = metadata.substr(start + 6, end - start - 6);
    return block.find("Connected to Game: [B]Yes[/B]") != block.npos &&
           block.find("Playing: [B]Yes[/B]") != block.npos;
}
struct Context {
    std::uint64_t server = 0, channel = 0;
    std::uint16_t self = 0;
    std::string serverUID;
    bool tfarActive = false;
    Millis observed = 0;
};
enum class Route { Bypass, DeathTrooper, Helmet };
class Core {
    struct Peer {
        std::string uid;
        int mode = -1;
        std::uint64_t sequence = 0;
        Millis received = 0, lastAudio = 0;
        Route lastRoute = Route::Bypass;
        int lastChannels = 0;
        deathTrooper::Effect death;
        helmetVoice::Effect helmet;
        void reset() noexcept { death.reset(); helmet.reset(); lastAudio = 0; lastRoute = Route::Bypass; lastChannels = 0; }
    };
    mutable std::mutex mutex_;
    Context context_;
    LocalState local_;
    bool haveLocal_ = false;
    std::unordered_map<std::uint16_t, std::unique_ptr<Peer>> peers_;
    bool localActive(Millis now) const noexcept {
        return haveLocal_ && local_.mode >= 0 && context_.server != 0 && context_.tfarActive &&
            local_.serverUID == context_.serverUID && fresh(now, local_.received, LocalTTL) &&
            fresh(now, context_.observed, ContextTTL);
    }
public:
    void setContext(Context context) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (context.server != context_.server || context.self != context_.self || context.channel != context_.channel ||
            context.serverUID != context_.serverUID || context.tfarActive != context_.tfarActive) {
            haveLocal_ = false; peers_.clear();
        }
        context_ = std::move(context);
    }
    void invalidate() { setContext({}); }
    void invalidateServer(std::uint64_t server) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (context_.server == server) { context_ = {}; haveLocal_ = false; peers_.clear(); }
    }
    void forget(std::uint64_t server, std::uint16_t client) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (server != context_.server) return;
        if (client == context_.self) { context_ = {}; haveLocal_ = false; peers_.clear(); }
        else peers_.erase(client);
    }
    bool acceptLocal(std::string_view raw, Millis now) {
        LocalState candidate;
        if (!parseLocal(raw, candidate)) return false;
        std::lock_guard<std::mutex> guard(mutex_);
        if (!context_.server || candidate.serverUID != context_.serverUID || !context_.tfarActive ||
            !fresh(now, context_.observed, ContextTTL)) return false;
        const bool interrupted = !haveLocal_ || !fresh(now, local_.received, LocalTTL) || local_.mission != candidate.mission;
        if (interrupted) peers_.clear();
        else if (candidate.mode != local_.mode || !(candidate.settings == local_.settings))
            for (auto& item : peers_) item.second->reset();
        candidate.received = now; local_ = std::move(candidate); haveLocal_ = true;
        return true;
    }
    bool acceptPeer(std::uint64_t server, std::uint16_t client, std::string_view authenticatedUID,
                    const PeerMessage& message, Millis now) {
        if (!client || !safeToken(authenticatedUID, 128, true) || !modeValid(message.mode) || !message.sequence) return false;
        // Allocations happen only on control events, never on the audio path.
        std::lock_guard<std::mutex> guard(mutex_);
        if (server != context_.server || client == context_.self || !localActive(now) || message.mission != local_.mission) return false;
        auto found = peers_.find(client);
        if (found != peers_.end() && found->second->uid == authenticatedUID &&
            fresh(now, found->second->received, RemoteTTL) && message.sequence <= found->second->sequence) return false;
        if (found == peers_.end()) {
            if (peers_.size() >= MaxPeers) return false;
            found = peers_.emplace(client, std::make_unique<Peer>()).first;
        }
        auto& peer = *found->second;
        if (peer.uid != authenticatedUID || peer.mode != message.mode || !fresh(now, peer.received, RemoteTTL)) peer.reset();
        peer.uid = authenticatedUID; peer.mode = message.mode; peer.sequence = message.sequence; peer.received = now;
        return true;
    }
    // Control-thread snapshot used for broadcasting; no TS API is called while locked.
    bool advertisement(Millis now, std::uint64_t& server, std::string& mission, int& mode) const {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!haveLocal_ || !context_.server || !fresh(now, context_.observed, ContextTTL)) return false;
        server = context_.server; mission = local_.mission; mode = localActive(now) ? local_.mode : -1;
        return true;
    }
    void prune(Millis now) {
        std::lock_guard<std::mutex> guard(mutex_);
        for (auto it = peers_.begin(); it != peers_.end();) {
            if (!fresh(now, it->second->received, RemoteTTL)) it = peers_.erase(it); else ++it;
        }
    }
    Route playback(std::uint64_t server, std::uint16_t client, short* samples, int frames, int channels, Millis now) noexcept {
        if (!samples || frames <= 0 || frames > 48000 || channels < 1 || channels > 2) return Route::Bypass;
        // Never block the realtime callback on IPC, plugin commands or allocation.
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) return Route::Bypass;
        auto it = peers_.find(client);
        if (it == peers_.end()) return Route::Bypass;
        auto& peer = *it->second;
        Route route = Route::Bypass;
        if (server == context_.server && client != context_.self && localActive(now) && fresh(now, peer.received, RemoteTTL)) {
            const bool decoder = local_.mode == 1 || local_.mode == 3;
            if (peer.mode == 3 && !decoder) route = Route::DeathTrooper;
            if (peer.mode == 4) route = Route::Helmet;
        }
        if (route == Route::Bypass) { peer.reset(); return route; }
        if (route != peer.lastRoute || channels != peer.lastChannels || !fresh(now, peer.lastAudio, AudioGap + 1)) peer.reset();
        const auto& p = local_.settings.values;
        if (route == Route::DeathTrooper)
            peer.death.process(samples, frames, channels, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10] != 0,p[11],p[12],p[13],p[14]);
        else peer.helmet.process(samples, frames, channels, p[15],p[16],p[17],p[18],p[19],p[20],p[21],p[22]);
        peer.lastAudio = now; peer.lastRoute = route; peer.lastChannels = channels;
        return route;
    }
};
} // namespace tfv
