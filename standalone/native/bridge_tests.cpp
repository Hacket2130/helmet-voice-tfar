#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <stdexcept>

using Extension = void (__stdcall*)(char*, int, const char*);
using Version = void (__stdcall*)(char*, int);
static int checks = 0;
static void check(bool value, const char* label) { if (!value) throw std::runtime_error(label); ++checks; }
static void roundtrip(Extension call, const char* request, const char* answer, const char* expected, DWORD delay = 0) {
    DWORD session = 0; check(ProcessIdToSessionId(GetCurrentProcessId(), &session) != 0, "session ID");
    std::wstring name = L"\\\\.\\pipe\\TFV_HelmetVoice_" + std::to_wstring(session);
    HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, nullptr);
    check(pipe != INVALID_HANDLE_VALUE, "test pipe available (never replace a live companion)");
    std::string received;
    std::thread server([&] {
        const BOOL connected = ConnectNamedPipe(pipe, nullptr);
        if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
            char bytes[4096]{}; DWORD count = 0;
            if (ReadFile(pipe, bytes, 4095, &count, nullptr)) {
                received.assign(bytes, count);
                if (delay) Sleep(delay);
                DWORD written = 0;
                WriteFile(pipe, answer, static_cast<DWORD>(std::strlen(answer)), &written, nullptr);
            }
        }
        DisconnectNamedPipe(pipe); CloseHandle(pipe);
    });
    char output[256]{};
    ULONGLONG start = GetTickCount64();
    call(output, sizeof output, request);
    ULONGLONG elapsed = GetTickCount64() - start;
    server.join();
    check(received == request, "exact no-NUL IPC payload");
    check(std::strcmp(output, expected) == 0, "bridge response");
    if (delay) check(elapsed < 180, "timeout returns before stalled peer");
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("supply bridge DLL path");
        HMODULE module = LoadLibraryA(argv[1]); check(module != nullptr, "load bridge DLL");
        auto call = reinterpret_cast<Extension>(GetProcAddress(module, "RVExtension"));
        auto version = reinterpret_cast<Version>(GetProcAddress(module, "RVExtensionVersion"));
        check(call != nullptr && version != nullptr, "Arma extension exports");
        char out[256]{}; version(out, sizeof out); check(std::strcmp(out, "TFV12 bridge 0.12.0") == 0, "bridge version");
        char guard[] = {'x','x','x'}; version(guard, 1); check(guard[0] == 0 && guard[1] == 'x', "version buffer bounds");
        call(nullptr, 0, nullptr); call(out, 0, nullptr); call(out, sizeof out, nullptr); check(std::strcmp(out,"ERR:INVALID") == 0,"null input");
        call(out,sizeof out,"OTHER"); check(std::strcmp(out,"ERR:INVALID") == 0,"unknown command");
        const std::string large(4096,'a'); call(out,sizeof out,large.c_str()); check(std::strcmp(out,"ERR:INVALID") == 0,"oversized command");
        call(out,sizeof out,"PING"); check(std::strcmp(out,"ERR:NO_PLUGIN") == 0,"absent companion bypass");
        roundtrip(call,"PING","TFV12","TFV12");
        roundtrip(call,"TFV12\tserver\tmission\t0\t1,2","OK","OK");
        roundtrip(call,"PING","unrecognized","ERR:REPLY");
        roundtrip(call,"PING","ERR:SERVER","ERR:SERVER");
        roundtrip(call,"PING","TFV12","ERR:TIMEOUT",200);
        FreeLibrary(module);
        std::cout << "PASS: " << checks << " bridge checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
