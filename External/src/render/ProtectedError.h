#pragma once
#include "CaptureProtection.h"
#include <algorithm>
#include <string>

namespace ProtectedError {
inline LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(w) == IDOK)) {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

inline HWND CreateHidden(const std::wstring& title, const std::wstring& message, HFONT& font) {
    const auto instance = GetModuleHandleW(nullptr);
    constexpr auto className = L"GoatProtectedError";
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = className;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HDC dc = GetDC(nullptr);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    RECT text{0, 0, 500, 0};
    DrawTextW(dc, message.c_str(), -1, &text, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
    ReleaseDC(nullptr, dc);
    POINT cursor{}; GetCursorPos(&cursor);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const int textHeight = (std::max)(70L, (std::min)(text.bottom, monitor.rcWork.bottom - monitor.rcWork.top - 180L));
    RECT bounds{0, 0, 580, textHeight + 90};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    AdjustWindowRectEx(&bounds, style, FALSE, exStyle);
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    HWND window = CreateWindowExW(exStyle, className, title.c_str(), style,
        monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
        monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
        width, height, nullptr, nullptr, instance, nullptr);
    if (!window) { if (font) DeleteObject(font); font = nullptr; return nullptr; }
    HWND icon = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_ICON,
        18, 24, 32, 32, window, nullptr, instance, nullptr);
    SendMessageW(icon, STM_SETICON, reinterpret_cast<WPARAM>(LoadIcon(nullptr, IDI_ERROR)), 0);
    HWND label = CreateWindowExW(0, L"STATIC", message.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        62, 22, 500, textHeight, window, nullptr, instance, nullptr);
    HWND button = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        476, textHeight + 48, 86, 28, window, reinterpret_cast<HMENU>(IDOK), instance, nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (!CaptureProtection::Apply(window, true)) {
        DestroyWindow(window);
        if (font) DeleteObject(font);
        font = nullptr;
        return nullptr;
    }
    return window;
}

inline bool Show(const std::wstring& title, const std::wstring& message) {
    HFONT font = nullptr;
    HWND window = CreateHidden(title, message, font);
    if (!window) return false;
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(GetDlgItem(window, IDOK));
    MSG msg{};
    while (IsWindow(window) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    if (font) DeleteObject(font);
    return true;
}
}
