#include "WindowIdentity.h"
#include <objbase.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#pragma comment(lib, "ole32.lib")

namespace {
std::filesystem::path LogFile() {
    wchar_t directory[32768]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
    if (!length || length >= 32768) {
        length = GetTempPathW(32768, directory);
        if (!length || length >= 32768) return {};
    }
    return std::filesystem::path(directory) / L"GoatGoatGoat" / L"diagnostics" / L"window-classes.log";
}

void Write(const std::string& line) noexcept {
    try {
        static std::mutex mutex;
        std::lock_guard<std::mutex> guard(mutex);
        const auto path = LogFile();
        if (path.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return;
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(file);
    } catch (...) {}
}
}

namespace WindowIdentity {
const char* BuildId() noexcept {
    static const auto value = [] {
        std::array<char, 37> result{};
        std::memcpy(result.data(), "unversioned", 11);
        const auto module = GetModuleHandleW(nullptr);
        const auto resource = FindResourceW(module, MAKEINTRESOURCEW(207), MAKEINTRESOURCEW(10));
        if (!resource || SizeofResource(module, resource) != 36) return result;
        const auto data = static_cast<const char*>(LockResource(LoadResource(module, resource)));
        if (!data) return result;
        for (int i = 0; i < 36; ++i) {
            const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
            const char c = data[i];
            if (dash ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return result;
        }
        std::memcpy(result.data(), data, 36);
        return result;
    }();
    return value.data();
}

void Generate(wchar_t (&name)[64]) noexcept {
    GUID id{};
    if (SUCCEEDED(CoCreateGuid(&id))) {
        swprintf_s(name, L"Wnd_%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
            static_cast<unsigned>(id.Data1), id.Data2, id.Data3,
            id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
            id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
    } else {
        static std::atomic<unsigned long> sequence{0};
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        swprintf_s(name, L"Wnd_%08lx%016llx%08lx", GetCurrentProcessId(),
            static_cast<unsigned long long>(counter.QuadPart), ++sequence);
    }
}

void RecordClass(const char* event, const char* meaning, const char* original,
    const wchar_t* name, DWORD error) noexcept {
    try {
        SYSTEMTIME time{};
        GetSystemTime(&time);
        char line[1024]{};
        const int length = snprintf(line, sizeof(line),
            "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ pid=%lu build=%s event=%s meaning=\"%s\" original=\"%s\" class=\"%ls\" win32=%lu\r\n",
            time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
            time.wMilliseconds, GetCurrentProcessId(), BuildId(), event, meaning, original,
            name ? name : L"", error);
        if (length > 0 && static_cast<size_t>(length) < sizeof(line)) Write(std::string(line, length));
    } catch (...) {}
}

void BeginSession() noexcept {
    RecordClass("session-start", "Local experimental build", "", nullptr);
}
}
