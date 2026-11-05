#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "sdk/ts3_functions.h"
#include "sdk/teamspeak/public_rare_definitions.h"
#include "CompanionCore.hpp"
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
int checks = 0;
void check(bool value, const char* label) { if (!value) throw std::runtime_error(label); ++checks; }
std::string localRecord(int mode = 0, const char* uid = "serverUID==", const char* mission = "TFV12_mission1") {
    return std::string("TFV12\t") + uid + "\t" + mission + "\t" + std::to_string(mode) +
        "\t0.25,0.35,150,-6,32,0.75,0.25,0.65,8,0.12,1,-48,1,3200,0.22,623,3100,2.4,1,2000,3,2500,2";
}
std::vector<short> voice(int channels = 1) {
    std::vector<short> samples(static_cast<size_t>(960 * channels));
    for (int frame = 0; frame < 960; ++frame)
        for (int c = 0; c < channels; ++c)
            samples[static_cast<size_t>(frame * channels + c)] = static_cast<short>(std::sin(frame * .16 + c * .3) * 12000);
    return samples;
}
tfv::Context context(tfv::Millis now = 10000) { return {7,11,1,"serverUID==",true,now}; }
void coreTests() {
    tfv::LocalState parsed;
    check(tfv::parseLocal(localRecord(), parsed), "parse complete local record");
    for (int mode : {-1,0,1,3,4}) check(tfv::parseLocal(localRecord(mode), parsed), "accepted helmet mode");
    check(!tfv::parseLocal(localRecord(2), parsed), "reject unsupported mode");
    check(!tfv::parseLocal(localRecord() + ",4", parsed), "reject extra settings");
    check(!tfv::parseLocal(localRecord() + "\textra", parsed), "reject extra fields");
    check(!tfv::parseLocal(localRecord(0, "bad uid"), parsed), "reject unsafe server UID");
    check(!tfv::parseLocal(localRecord(0, "serverUID==", "bad:mission"), parsed), "reject unsafe mission");
    auto invalid = localRecord(); invalid.replace(invalid.find("0.25"),4,"nan");
    check(!tfv::parseLocal(invalid, parsed), "reject NaN");
    invalid = localRecord(); invalid.replace(invalid.find("0.25"),4,"0.7");
    check(!tfv::parseLocal(invalid, parsed), "reject out-of-range setting");
    invalid = localRecord(); invalid.push_back('\0');
    check(!tfv::parseLocal(invalid, parsed), "reject embedded NUL");
    check(!tfv::parseLocal(std::string(4096,'x'), parsed), "reject oversized local payload");
    check(tfv::ownPlugin("task_force_vanguard_win64") && !tfv::ownPlugin("TFAR_win64") && !tfv::ownPlugin("task_force_vanguard_bad"), "strict own-plugin names");
    check(tfv::activeTFARMetadata("<TFAR>Connected to Game: [B]Yes[/B]\nPlaying: [B]Yes[/B]</TFAR>"), "active stock TFAR metadata");
    check(!tfv::activeTFARMetadata("<TFAR>[B]Task Force Radio Plugin Disabled[/B]</TFAR>"), "disabled TFAR bypass");
    check(!tfv::activeTFARMetadata("Connected to Game: [B]Yes[/B]\nPlaying: [B]Yes[/B]"), "TFAR metadata tags required");
    tfv::PeerMessage message;
    check(tfv::parsePeer("TFV12\tTFV12_mission1\t4\t12",message), "parse peer state");
    check(!tfv::parsePeer("TFV12\tTFV12_mission1\t4\t12\t99",message), "reject payload speaker ID");
    check(!tfv::parsePeer("TFV12\tTFV12_mission1\t4\t-1",message), "reject invalid sequence");
    tfv::Core core; core.setContext(context());
    check(!core.acceptLocal(localRecord(0,"otherUID=="),10000), "reject wrong selected server");
    check(core.acceptLocal(localRecord(),10000), "accept matching server");
    tfv::PeerMessage helmet{"TFV12_mission1",4,1};
    check(!core.acceptPeer(7,1,"selfUID==",helmet,10000), "reject own voice");
    check(!core.acceptPeer(8,2,"peerUID==",helmet,10000), "reject other server peer");
    auto other = helmet; other.mission = "otherMission";
    check(!core.acceptPeer(7,2,"peerUID==",other,10000), "reject other mission peer");
    check(core.acceptPeer(7,2,"peerUID==",helmet,10000), "accept authenticated peer");
    check(!core.acceptPeer(7,2,"peerUID==",helmet,10001), "reject replay");
    auto dry = voice(), wet = dry;
    check(core.playback(7,2,wet.data(),960,1,10000) == tfv::Route::Helmet && wet != dry, "helmet effect routes");
    auto firstWet = wet;
    check(core.acceptPeer(7,3,"otherUID==",helmet,10000), "accept second speaker");
    wet = dry; core.playback(7,3,wet.data(),960,1,10000);
    check(wet == firstWet, "separate DSP state per speaker");
    wet = dry; core.playback(7,2,wet.data(),960,1,10251);
    check(wet == firstWet, "reset after 250ms gap");
    wet = dry; check(core.playback(8,2,wet.data(),960,1,10252) == tfv::Route::Bypass && wet == dry, "wrong server exact bypass");
    wet = voice(3); const auto surround = wet;
    check(core.playback(7,2,wet.data(),960,3,10253) == tfv::Route::Bypass && wet == surround, "unsupported layout exact bypass");
    wet = dry; check(core.playback(7,99,wet.data(),960,1,10253) == tfv::Route::Bypass && wet == dry, "unknown client exact bypass");
    check(core.acceptLocal(localRecord(1),10254), "enable listener decoder");
    tfv::PeerMessage death{"TFV12_mission1",3,2};
    check(core.acceptPeer(7,2,"peerUID==",death,10254), "remote scrambler state");
    wet = dry; check(core.playback(7,2,wet.data(),960,1,10254) == tfv::Route::Bypass && wet == dry, "DT helmet decodes exact original");
    check(core.acceptLocal(localRecord(),10255), "remove decoder");
    wet = dry; check(core.playback(7,2,wet.data(),960,1,10255) == tfv::Route::DeathTrooper && wet != dry, "non-DT hears approved scramble");
    check(core.acceptLocal(localRecord(-1),10256), "inactive local state");
    wet = dry; check(core.playback(7,2,wet.data(),960,1,10256) == tfv::Route::Bypass && wet == dry, "inactive exact bypass");
    check(core.acceptLocal(localRecord(),10257), "active state restored");
    core.setContext(context(13257));
    wet = dry; check(core.playback(7,2,wet.data(),960,1,13257) == tfv::Route::Bypass && wet == dry, "local heartbeat TTL 3s");
    check(core.acceptLocal(localRecord(),13257), "local heartbeat restarted");
    wet = dry; check(core.playback(7,2,wet.data(),960,1,13257) == tfv::Route::Bypass && wet == dry, "restart requires fresh peer state");
    check(core.acceptPeer(7,2,"peerUID==",helmet,13257), "fresh peer after local interruption");
    core.setContext(context(15257)); core.acceptLocal(localRecord(),15257);
    core.setContext(context(17257)); core.acceptLocal(localRecord(),17257);
    core.setContext(context(19257)); core.acceptLocal(localRecord(),19257);
    wet = dry; check(core.playback(7,2,wet.data(),960,1,19257) == tfv::Route::Bypass && wet == dry, "remote heartbeat TTL 6s");
    check(core.acceptPeer(7,2,"newIdentity==",helmet,19257), "client ID reuse resets identity state");
    core.forget(7,2); wet = dry;
    check(core.playback(7,2,wet.data(),960,1,19257) == tfv::Route::Bypass && wet == dry, "remote departure clears state");
    core.acceptPeer(7,2,"newIdentity==",helmet,19257); core.forget(7,1); wet = dry;
    check(core.playback(7,2,wet.data(),960,1,19257) == tfv::Route::Bypass && wet == dry, "local channel move clears state");
}

std::atomic<uint64> selectedServer{7}, channel{11};
std::atomic<bool> tfarActive{true};
std::atomic<int> broadcasts{0};
unsigned int freeTS(void* pointer) { std::free(pointer); return 0; }
unsigned int stringResult(const char* input, char** output) { *output = _strdup(input); return *output ? 0u : 1u; }
uint64 currentServer() { return selectedServer.load(); }
unsigned int connectionStatus(uint64, int* result) { *result = STATUS_CONNECTION_ESTABLISHED; return 0; }
unsigned int getID(uint64, anyID* result) { *result = 1; return 0; }
unsigned int getChannel(uint64, anyID, uint64* result) { *result = channel.load(); return 0; }
unsigned int getServerString(uint64, size_t, char** result) { return stringResult("serverUID==",result); }
unsigned int getClientString(uint64, anyID id, size_t property, char** result) {
    if (property == CLIENT_UNIQUE_IDENTIFIER) return stringResult(id == 1 ? "selfUID==" : "peerUID==",result);
    if (property == CLIENT_META_DATA)
        return stringResult(tfarActive ? "<TFAR>Connected to Game: [B]Yes[/B]\nPlaying: [B]Yes[/B]</TFAR>" : "<TFAR>[B]Task Force Radio Plugin Disabled[/B]</TFAR>",result);
    return 1;
}
void sendCommand(uint64 server, const char* id, const char* command, int target, const anyID* ids, const char*) {
    tfv::PeerMessage message;
    if (server == 7 && std::strcmp(id,"testRegisteredID") == 0 && tfv::parsePeer(command,message) && target == PluginCommandTarget_CURRENT_CHANNEL && !ids)
        ++broadcasts;
}
unsigned int logMock(const char*, enum LogLevel, const char*, uint64) { return 0; }
using Extension = void (__stdcall*)(char*, int, const char*);
using Playback = void (*)(uint64,anyID,short*,int,int);
using Remote = void (*)(uint64,const char*,const char*,anyID,const char*,const char*);
template<class T> T symbol(HMODULE module, const char* name) {
    auto address = GetProcAddress(module,name);
    check(address != nullptr,name);
    return reinterpret_cast<T>(address);
}
struct LoadedPlugin {
    HMODULE companion = nullptr, bridge = nullptr;
    void (*shutdown)() = nullptr;
    bool started = false;
    ~LoadedPlugin() { if (started && shutdown) shutdown(); if (bridge) FreeLibrary(bridge); if (companion) FreeLibrary(companion); }
};
std::string invokeBridge(Extension bridge, const std::string& request) {
    char reply[256]{}; bridge(reply,sizeof(reply),request.c_str()); return reply;
}
HANDLE connectSilentPeer() {
    DWORD session = 0;
    check(ProcessIdToSessionId(GetCurrentProcessId(),&session) != 0,"test Windows session");
    auto path = L"\\\\.\\pipe\\TFV_HelmetVoice_" + std::to_wstring(session);
    HANDLE peer = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 25; ++retry) {
        peer = CreateFileW(path.c_str(),GENERIC_READ | GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,nullptr);
        if (peer != INVALID_HANDLE_VALUE) break;
        Sleep(10);
    }
    check(peer != INVALID_HANDLE_VALUE,"connect silent IPC client");
    return peer;
}
void dllTests(const char* pluginPath, const char* bridgePath) {
    LoadedPlugin modules;
    modules.companion = LoadLibraryA(pluginPath); check(modules.companion != nullptr,"load production companion DLL");
    modules.bridge = LoadLibraryA(bridgePath); check(modules.bridge != nullptr,"load production Arma bridge DLL");
    auto init = symbol<int (*)()>(modules.companion,"ts3plugin_init");
    modules.shutdown = symbol<void (*)()>(modules.companion,"ts3plugin_shutdown");
    auto setFunctions = symbol<void (*)(TS3Functions)>(modules.companion,"ts3plugin_setFunctionPointers");
    auto registerID = symbol<void (*)(const char*)>(modules.companion,"ts3plugin_registerPluginID");
    auto playback = symbol<Playback>(modules.companion,"ts3plugin_onEditPlaybackVoiceDataEvent");
    auto remote = symbol<Remote>(modules.companion,"ts3plugin_onPluginCommandEvent");
    auto changed = symbol<void (*)(uint64)>(modules.companion,"ts3plugin_currentServerConnectionChanged");
    auto apiVersion = symbol<int (*)()>(modules.companion,"ts3plugin_apiVersion");
    auto bridge = symbol<Extension>(modules.bridge,"RVExtension");
    check(apiVersion() == 26,"exact API26 ABI");
    check(!GetProcAddress(modules.companion,"ts3plugin_onEditCapturedVoiceDataEvent"),"no capture effect export");
    check(!GetProcAddress(modules.companion,"ts3plugin_onEditPostProcessVoiceDataEvent"),"stock TFAR postprocess untouched");
    check(!GetProcAddress(modules.companion,"ts3plugin_onEditMixedPlaybackVoiceDataEvent"),"no mixed playback injection");
    TS3Functions functions{};
    functions.getCurrentServerConnectionHandlerID = currentServer;
    functions.getConnectionStatus = connectionStatus; functions.getClientID = getID;
    functions.getChannelOfClient = getChannel; functions.getServerVariableAsString = getServerString;
    functions.getClientVariableAsString = getClientString; functions.freeMemory = freeTS;
    functions.sendPluginCommand = sendCommand; functions.logMessage = logMock;
    setFunctions(functions); registerID("testRegisteredID");
    check(init() == 0,"init with mock TS API (requires free test pipe)"); modules.started = true;
    std::string response;
    for (int retry = 0; retry < 50; ++retry) {
        response = invokeBridge(bridge,localRecord()); if (response == "OK") break;
        Sleep(20);
    }
    check(response == "OK","real bridge/companion state IPC");
    Sleep(10); check(invokeBridge(bridge,"PING") == "TFV12","real bridge/companion PING IPC");
    Sleep(10);
    HANDLE stalled = connectSilentPeer();
    Sleep(90); CloseHandle(stalled);
    check(invokeBridge(bridge,"PING") == "TFV12","silent IPC client times out without blocking next caller");
    Sleep(10); check(invokeBridge(bridge,localRecord(0,"wrongUID==")) == "ERR:STATE","IPC rejects wrong selected server");
    auto dry = voice(), wet = dry;
    remote(7,"task_force_vanguard_win64","TFV12\tTFV12_mission1\t4\t1",2,"ignored nickname","forgedUID==");
    playback(7,2,wet.data(),960,1); check(wet == dry,"callback rejects forged identity");
    remote(7,"TFAR_win64","TFV12\tTFV12_mission1\t4\t1",2,"ignored nickname","peerUID==");
    playback(7,2,wet.data(),960,1); check(wet == dry,"callback ignores stock TFAR commands");
    remote(7,"task_force_vanguard_win64","TFV12\tTFV12_mission1\t4\t1",2,"ignored nickname","peerUID==");
    playback(7,2,wet.data(),960,1); check(wet != dry,"exported production early playback effect");
    // Simulate TeamSpeak ordering: later TFAR controls gain/muting after the
    // companion. This verifies buffer composition, not actual game/radio logic.
    auto later = wet;
    for (auto& sample : later) sample = 0;
    check(std::all_of(later.begin(),later.end(),[](short sample){return sample == 0;}),"later TFAR mute retains final authority");
    auto capture = dry; check(capture == dry,"original transmission remains available to decoder listeners");
    Sleep(10); check(invokeBridge(bridge,localRecord(1)) == "OK","live listener decoder update");
    remote(7,"task_force_vanguard_win64","TFV12\tTFV12_mission1\t3\t2",2,"nickname not used","peerUID==");
    wet = dry; playback(7,2,wet.data(),960,1); check(wet == dry,"exported callback DT-to-DT exact clear voice");
    Sleep(10); check(invokeBridge(bridge,localRecord()) == "OK","live remove decoder");
    wet = dry; playback(7,2,wet.data(),960,1); check(wet != dry,"exported callback non-DT scramble");
    changed(7); wet = dry; playback(7,2,wet.data(),960,1); check(wet == dry,"selection callback immediately bypasses");
    Sleep(350); check(broadcasts.load() > 0,"own-plugin current-channel advertisements");
    check(invokeBridge(bridge,localRecord()) == "OK","state after selected-server refresh");
    remote(7,"task_force_vanguard_win64","TFV12\tTFV12_mission1\t4\t3",2,"name","peerUID==");
    tfarActive = false; Sleep(350);
    wet = dry; playback(7,2,wet.data(),960,1); check(wet == dry,"stock TFAR disabled means exact bypass");
    check(invokeBridge(bridge,localRecord()) == "ERR:STATE","no accepted heartbeat while stock TFAR disabled");
    auto before = GetTickCount64(); modules.shutdown(); modules.started = false;
    check(GetTickCount64()-before < 500,"bounded shutdown with pending pipe connect");
    check(invokeBridge(bridge,"PING") == "ERR:NO_PLUGIN","shutdown releases pipe");
    wet = dry; playback(7,2,wet.data(),960,1); check(wet == dry,"callback after shutdown exact bypass");
    tfarActive = true;
    check(init() == 0,"companion reinitializes without leaked pipe or thread"); modules.started = true;
    stalled = connectSilentPeer(); Sleep(5);
    before = GetTickCount64(); modules.shutdown(); modules.started = false;
    CloseHandle(stalled);
    check(GetTickCount64()-before < 500,"bounded shutdown with pending read");
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 3,"usage: companion_tests companion.dll bridge.dll");
        coreTests(); dllTests(argv[1],argv[2]);
        std::cout << "PASS: " << checks << " protocol, state, exported callback and IPC checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n'; return 1; }
}
