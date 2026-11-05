#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstring>

// Loads the actual DLL without calling ts3plugin_init, connecting to a server,
// recording audio, or changing TeamSpeak. Checks exports and outside-game bypass.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    HMODULE dll = LoadLibraryA(argv[1]);
    if (!dll) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 1; }
    using TextFn = const char*(*)();
    auto version = reinterpret_cast<TextFn>(GetProcAddress(dll, "ts3plugin_version"));
    auto name = reinterpret_cast<TextFn>(GetProcAddress(dll, "ts3plugin_name"));
    using AudioFn = void(*)(unsigned long long, unsigned short, short*, int, int, const unsigned int*, unsigned int*);
    auto audio = reinterpret_cast<AudioFn>(GetProcAddress(dll, "ts3plugin_onEditPostProcessVoiceDataEvent"));
    if (!version || !name || !audio) {std::cerr << "Missing export\n"; FreeLibrary(dll); return 1;}
    std::cout << "Plugin: " << name() << " | " << version() << '\n';
    if (!std::strstr(version(), "dtr.11")) {FreeLibrary(dll); return 1;}
    short samples[] = {1000, -2000, 3000, -4000};
    const short expected[] = {1000, -2000, 3000, -4000};
    audio(0, 1, samples, 2, 2, nullptr, nullptr);
    const bool unchanged = std::memcmp(samples, expected, sizeof(samples)) == 0;
    std::cout << (unchanged ? "PASS" : "FAIL") << " actual DLL outside-game audio bypass\n";
    FreeLibrary(dll);
    return unchanged ? 0 : 1;
}
