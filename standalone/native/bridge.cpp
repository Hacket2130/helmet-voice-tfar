#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
constexpr DWORD Capacity = 4096;
constexpr DWORD TimeoutMs = 60;

void reply(char* output, int size, const char* message) noexcept {
    if (!output || size <= 0) return;
    const size_t count = (std::min)(std::strlen(message), static_cast<size_t>(size - 1));
    if (count) std::memcpy(output, message, count);
    output[count] = '\0';
}

void exchange(char* output, int size, const char* command) {
    if (!output || size <= 0) return;
    reply(output, size, "ERR:INVALID");
    if (!command) return;
    const size_t length = strnlen_s(command, Capacity);
    if (!length || length >= Capacity) return;
    if (std::strcmp(command, "PING") != 0 && std::strncmp(command, "TFV12\t", 6) != 0) return;
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) { reply(output, size, "ERR:SESSION"); return; }
    const std::wstring pipe = L"\\\\.\\pipe\\TFV_HelmetVoice_" + std::to_wstring(session);
    HANDLE file = CreateFileW(pipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (file == INVALID_HANDLE_VALUE) { reply(output, size, "ERR:NO_PLUGIN"); return; }
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(file, &mode, nullptr, nullptr)) {
        CloseHandle(file); reply(output, size, "ERR:PIPE_MODE"); return;
    }
    OVERLAPPED pending{};
    pending.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!pending.hEvent) { CloseHandle(file); reply(output, size, "ERR:EVENT"); return; }
    char response[Capacity]{};
    DWORD received = 0;
    BOOL success = TransactNamedPipe(file, const_cast<char*>(command), static_cast<DWORD>(length),
        response, Capacity - 1, &received, &pending);
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(pending.hEvent, TimeoutMs) == WAIT_OBJECT_0) {
            success = GetOverlappedResult(file, &pending, &received, FALSE);
        } else {
            CancelIoEx(file, &pending);
            // Complete cancellation before releasing buffers used by the kernel.
            GetOverlappedResult(file, &pending, &received, TRUE);
            CloseHandle(pending.hEvent); CloseHandle(file);
            reply(output, size, "ERR:TIMEOUT"); return;
        }
    }
    CloseHandle(pending.hEvent); CloseHandle(file);
    if (!success || !received || received >= Capacity) { reply(output, size, "ERR:IO"); return; }
    response[received] = '\0';
    if (std::strcmp(response, "TFV12") != 0 && std::strcmp(response, "OK") != 0 && std::strncmp(response, "ERR:", 4) != 0) {
        reply(output, size, "ERR:REPLY"); return;
    }
    reply(output, size, response);
}
}

extern "C" __declspec(dllexport) void __stdcall RVExtensionVersion(char* output, int size) {
    reply(output, size, "TFV12 bridge 0.12.0");
}
extern "C" __declspec(dllexport) void __stdcall RVExtension(char* output, int size, const char* command) {
    try { exchange(output, size, command); }
    catch (...) { reply(output, size, "ERR:INTERNAL"); }
}
