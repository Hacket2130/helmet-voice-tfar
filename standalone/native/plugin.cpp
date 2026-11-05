#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#include "sdk/ts3_functions.h"
#include "sdk/teamspeak/public_rare_definitions.h"
#include "CompanionCore.hpp"
#include <atomic>
#include <thread>

#pragma comment(lib, "Advapi32.lib")
#define TFV_EXPORT extern "C" __declspec(dllexport)

namespace {
TS3Functions ts{};
tfv::Core core;
std::atomic<bool> running{false};
std::atomic<std::uint64_t> contextEpoch{0};
std::thread worker;
HANDLE stopEvent = nullptr;
HANDLE pipeHandle = INVALID_HANDLE_VALUE;
DWORD windowsSession = 0;
std::mutex idMutex;
std::string pluginID;

// This is the sole PCM entry point. There are deliberately no capture,
// post-process or mixed-playback exports. Stock TFAR owns those later stages.
void earlyPlayback(uint64 server, anyID client, short* samples, int frames, int channels, tfv::Millis now) noexcept {
    if (running.load(std::memory_order_acquire)) core.playback(server, client, samples, frames, channels, now);
}
void log(const char* message, LogLevel level = LogLevel_INFO) noexcept {
    if (ts.logMessage) ts.logMessage(message, level, "Task force Vanguard", 0);
}
std::string clientString(uint64 server, anyID client, int flag) {
    char* value = nullptr;
    if (ts.getClientVariableAsString(server, client, flag, &value) != 0 || !value) return {};
    std::string result(value); ts.freeMemory(value); return result;
}
tfv::Context queryContext() {
    tfv::Context context;
    context.server = ts.getCurrentServerConnectionHandlerID();
    int status = 0;
    if (!context.server || ts.getConnectionStatus(context.server, &status) || status != STATUS_CONNECTION_ESTABLISHED ||
        ts.getClientID(context.server, &context.self) || ts.getChannelOfClient(context.server, context.self, &context.channel)) return {};
    char* uid = nullptr;
    if (ts.getServerVariableAsString(context.server, VIRTUALSERVER_UNIQUE_IDENTIFIER, &uid) || !uid) return {};
    context.serverUID = uid; ts.freeMemory(uid);
    if (!tfv::safeToken(context.serverUID, 128, true)) return {};
    context.tfarActive = tfv::activeTFARMetadata(clientString(context.server, context.self, CLIENT_META_DATA));
    context.observed = tfv::nowMillis();
    return context;
}
struct ControlPulse {
    tfv::Millis lastPoll = 0, lastSend = 0;
    std::uint64_t lastServer = 0, sequence = 0;
    std::string lastMission;
    int lastMode = -2;
    void operator()() {
        auto now = tfv::nowMillis();
        if (!lastPoll || now - lastPoll >= 250) {
            const auto epoch = contextEpoch.load();
            auto context = queryContext();
            // A move/disconnect callback must not be undone by an older query.
            if (epoch == contextEpoch.load() && context.server == ts.getCurrentServerConnectionHandlerID())
                core.setContext(std::move(context));
            lastPoll = now; core.prune(now);
        }
        std::uint64_t server = 0;
        std::string mission;
        int mode = -1;
        if (!core.advertisement(now, server, mission, mode)) { lastMode = -2; return; }
        if (server == lastServer && mission == lastMission && mode == lastMode && now - lastSend < 2000) return;
        std::string id;
        { std::lock_guard<std::mutex> lock(idMutex); id = pluginID; }
        if (id.empty()) return;
        // Time-derived initial sequence survives a plugin reload; strict
        // per-sender monotonic checks discard duplicate and reordered messages.
        sequence = std::max(sequence + 1, now * 1000);
        auto command = "TFV12\t" + mission + "\t" + std::to_string(mode) + "\t" + std::to_string(sequence);
        ts.sendPluginCommand(server, id.c_str(), command.c_str(), PluginCommandTarget_CURRENT_CHANNEL, nullptr, nullptr);
        lastServer = server; lastMission = mission; lastMode = mode; lastSend = now;
    }
};

HANDLE makePipe() {
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &windowsSession)) return INVALID_HANDLE_VALUE;
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return INVALID_HANDLE_VALUE;
    DWORD required = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &required);
    std::vector<unsigned char> tokenBuffer(required);
    if (!required || !GetTokenInformation(token, TokenUser, tokenBuffer.data(), required, &required)) {
        CloseHandle(token); return INVALID_HANDLE_VALUE;
    }
    CloseHandle(token);
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(tokenBuffer.data())->User.Sid, &sid)) return INVALID_HANDLE_VALUE;
    const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    auto name = L"\\\\.\\pipe\\TFV_HelmetVoice_" + std::to_wstring(windowsSession);
    HANDLE result = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 4096, 4096, 0, &attributes);
    LocalFree(descriptor);
    return result;
}

// Connect waits run the control heartbeat. Every connected peer has a 40 ms
// read/write deadline. Stop interrupts all waits. Cancellation is drained before
// reusing OVERLAPPED storage, as required by Win32 (no dangling stack pointer).
bool awaitOperation(OVERLAPPED& operation, DWORD timeout, DWORD& transferred, ControlPulse& pulse) {
    const auto started = GetTickCount64();
    HANDLE events[2]{stopEvent, operation.hEvent};
    for (;;) {
        auto elapsed = GetTickCount64() - started;
        if (timeout != INFINITE && elapsed >= timeout) break;
        DWORD slice = timeout == INFINITE ? 50 : static_cast<DWORD>(std::min<ULONGLONG>(50, timeout - elapsed));
        DWORD result = WaitForMultipleObjects(2, events, FALSE, slice);
        if (result == WAIT_OBJECT_0 + 1) return GetOverlappedResult(pipeHandle, &operation, &transferred, FALSE) != FALSE;
        if (result != WAIT_TIMEOUT) break;
        if (timeout == INFINITE) pulse();
    }
    CancelIoEx(pipeHandle, &operation);
    GetOverlappedResult(pipeHandle, &operation, &transferred, TRUE);
    return false;
}
void pipeLoop() noexcept {
    HANDLE ioEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ioEvent) { running = false; return; }
    ControlPulse pulse;
    try {
        while (WaitForSingleObject(stopEvent, 0) == WAIT_TIMEOUT) {
            pulse();
            OVERLAPPED operation{}; operation.hEvent = ioEvent;
            ResetEvent(ioEvent);
            DWORD transferred = 0;
            BOOL connected = ConnectNamedPipe(pipeHandle, &operation);
            DWORD error = connected ? ERROR_SUCCESS : GetLastError();
            bool ready = connected || error == ERROR_PIPE_CONNECTED;
            if (error == ERROR_IO_PENDING) ready = awaitOperation(operation, INFINITE, transferred, pulse);
            if (!ready) { DisconnectNamedPipe(pipeHandle); continue; }
            ULONG peerSession = 0;
            if (!GetNamedPipeClientSessionId(pipeHandle, &peerSession) || peerSession != windowsSession) {
                DisconnectNamedPipe(pipeHandle); continue;
            }
            char buffer[4096]{};
            operation = {}; operation.hEvent = ioEvent; ResetEvent(ioEvent);
            BOOL read = ReadFile(pipeHandle, buffer, sizeof(buffer), &transferred, &operation);
            if (!read && GetLastError() == ERROR_IO_PENDING) read = awaitOperation(operation, 40, transferred, pulse);
            if (read && transferred > 0 && transferred <= tfv::MaxRecord) {
                std::string_view request(buffer, transferred);
                const char* reply = request == "PING" ? "TFV12" : (core.acceptLocal(request, tfv::nowMillis()) ? "OK" : "ERR:STATE");
                operation = {}; operation.hEvent = ioEvent; ResetEvent(ioEvent);
                DWORD bytes = 0;
                if (!WriteFile(pipeHandle, reply, static_cast<DWORD>(std::char_traits<char>::length(reply)), &bytes, &operation) && GetLastError() == ERROR_IO_PENDING)
                    awaitOperation(operation, 40, bytes, pulse);
                pulse();
            }
            // Never FlushFileBuffers here: it waits indefinitely for a reader.
            DisconnectNamedPipe(pipeHandle);
        }
    } catch (...) { log("Companion control worker stopped; voice effects bypass.", LogLevel_ERROR); }
    running = false; core.invalidate();
    CloseHandle(ioEvent);
}
void forgetClient(uint64 server, anyID client) noexcept {
    try { ++contextEpoch; core.forget(server, client); } catch (...) {}
}
} // namespace

TFV_EXPORT const char* ts3plugin_name() { return "Task force Vanguard TFAR addon"; }
TFV_EXPORT const char* ts3plugin_version() { return "0.12.0"; }
TFV_EXPORT const char* ts3plugin_author() { return "Hacket2130"; }
TFV_EXPORT const char* ts3plugin_description() { return "Optional helmet voice companion for stock TFAR Beta. Enable alongside TFAR. Settings and helmet detection are provided by the separate Arma addon."; }
TFV_EXPORT int ts3plugin_apiVersion() { return 26; }
TFV_EXPORT int ts3plugin_requestAutoload() { return 0; }
TFV_EXPORT int ts3plugin_offersConfigure() { return PLUGIN_OFFERS_NO_CONFIGURE; }
TFV_EXPORT void ts3plugin_setFunctionPointers(const TS3Functions functions) { ts = functions; }
TFV_EXPORT void ts3plugin_registerPluginID(const char* id) {
    try { std::lock_guard<std::mutex> lock(idMutex); pluginID = id ? id : ""; } catch (...) {}
}
TFV_EXPORT int ts3plugin_init() {
    if (running || worker.joinable()) return 1;
    if (!ts.getCurrentServerConnectionHandlerID || !ts.getConnectionStatus || !ts.getClientID || !ts.getChannelOfClient ||
        !ts.getClientVariableAsString || !ts.getServerVariableAsString || !ts.freeMemory || !ts.sendPluginCommand) return 1;
    try {
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        pipeHandle = makePipe();
        if (!stopEvent || pipeHandle == INVALID_HANDLE_VALUE) {
            log("Cannot open the per-user companion pipe; another TeamSpeak instance may own it.", LogLevel_ERROR);
            if (stopEvent) CloseHandle(stopEvent);
            if (pipeHandle != INVALID_HANDLE_VALUE) CloseHandle(pipeHandle);
            stopEvent = nullptr; pipeHandle = INVALID_HANDLE_VALUE; return 1;
        }
        running = true;
        worker = std::thread(pipeLoop);
        log("Standalone companion enabled. Stock TFAR remains responsible for radios, distance and muting.");
        return 0;
    } catch (...) {
        running = false;
        if (stopEvent) CloseHandle(stopEvent);
        if (pipeHandle != INVALID_HANDLE_VALUE) CloseHandle(pipeHandle);
        stopEvent = nullptr; pipeHandle = INVALID_HANDLE_VALUE; return 1;
    }
}
TFV_EXPORT void ts3plugin_shutdown() {
    running = false;
    if (stopEvent) SetEvent(stopEvent);
    if (worker.joinable()) worker.join();
    core.invalidate();
    if (pipeHandle != INVALID_HANDLE_VALUE) CloseHandle(pipeHandle);
    if (stopEvent) CloseHandle(stopEvent);
    pipeHandle = INVALID_HANDLE_VALUE; stopEvent = nullptr;
    std::lock_guard<std::mutex> lock(idMutex); pluginID.clear();
}
TFV_EXPORT void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 server, anyID client, short* samples, int frames, int channels) {
    earlyPlayback(server, client, samples, frames, channels, tfv::nowMillis());
}
TFV_EXPORT void ts3plugin_onPluginCommandEvent(uint64 server, const char* name, const char* command, anyID invoker,
                                             const char*, const char* invokerUID) {
    if (!running || !name || !command || !invokerUID) return;
    try {
        if (!tfv::ownPlugin(std::string_view(name, strnlen_s(name, 65)))) return;
        tfv::PeerMessage message;
        if (!tfv::parsePeer(std::string_view(command, strnlen_s(command, 129)), message)) return;
        auto uid = std::string_view(invokerUID, strnlen_s(invokerUID, 129));
        if (!tfv::safeToken(uid, 128, true) || server != ts.getCurrentServerConnectionHandlerID()) return;
        anyID self = 0; uint64 ownChannel = 0, peerChannel = 0;
        if (ts.getClientID(server, &self) || self == invoker || ts.getChannelOfClient(server, self, &ownChannel) ||
            ts.getChannelOfClient(server, invoker, &peerChannel) || !ownChannel || ownChannel != peerChannel ||
            clientString(server, invoker, CLIENT_UNIQUE_IDENTIFIER) != uid) return;
        core.acceptPeer(server, invoker, uid, message, tfv::nowMillis());
    } catch (...) { /* Invalid/unavailable state must leave voice untouched. */ }
}
TFV_EXPORT void ts3plugin_currentServerConnectionChanged(uint64) { ++contextEpoch; core.invalidate(); }
TFV_EXPORT void ts3plugin_onConnectStatusChangeEvent(uint64 server, int, unsigned int) { ++contextEpoch; core.invalidateServer(server); }
TFV_EXPORT void ts3plugin_onClientMoveEvent(uint64 s, anyID c, uint64, uint64, int, const char*) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientMoveSubscriptionEvent(uint64 s, anyID c, uint64, uint64, int) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientMoveTimeoutEvent(uint64 s, anyID c, uint64, uint64, int, const char*) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientMoveMovedEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, const char*) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientKickFromChannelEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, const char*) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientKickFromServerEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, const char*) { forgetClient(s,c); }
TFV_EXPORT void ts3plugin_onClientBanFromServerEvent(uint64 s, anyID c, uint64, uint64, int, anyID, const char*, const char*, uint64, const char*) { forgetClient(s,c); }
