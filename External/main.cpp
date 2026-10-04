#include <cstdint>
#include <windows.h>
#include <string>
#include "src/core/app/app.h"
#include "src/core/app/RuntimePaths.h"
#include "src/render/StartupLoader.h"
#include "src/render/menu/UiText.h"
#include "src/render/ProtectedError.h"

static void LogLine(const char* text) {
    if (HANDLE h = CreateFileW(RuntimePaths::File(L"goatgoatgoat.log").c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr); h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, nullptr, FILE_END);
        DWORD written = 0;
        WriteFile(h, text, (DWORD)lstrlenA(text), &written, nullptr);
        WriteFile(h, "\r\n", 2, &written, nullptr);
        CloseHandle(h);
    }
}

static void LogWideLine(const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return;
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    LogLine(utf8.c_str());
}

std::int32_t WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, L"Local\\TheRogCog1Special.SingleInstance");
    if (!singleInstance || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (singleInstance)
            CloseHandle(singleInstance);
        return 0;
    }

    UiText::Load();
    LogLine("goatgoatgoatgoatgoatgoat - waiting for roblox...");
    const std::int32_t code = StartupLoader::Run([]{return App::init();}) ? App::Run() :
        (App::StartupError().empty() && StartupLoader::Error().empty() ? 0 : 1);
    StartupLoader::Dismiss();
    if (code != 0) {
        char buf[64];
        wsprintfA(buf, "exited with code %d", code);
        LogLine(buf);
        const std::wstring reason = App::StartupError().empty() ? StartupLoader::Error() : App::StartupError();
        if (!reason.empty()) LogWideLine(reason);
        std::wstring message = UiText::language==0
            ? L"Programmet kunne ikke starte." : L"The external could not start.";
        message += reason.empty() ? L"\nCheck the startup log for details." : L"\n\n" + reason;
        if (!ProtectedError::Show(L"goatgoatgoatgoatgoatgoat", message))
            LogLine("The error window was not shown because Windows could not protect it from capture.");
    }
    ReleaseMutex(singleInstance);
    CloseHandle(singleInstance);
    return code;
}
