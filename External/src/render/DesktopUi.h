#pragma once
#include "CaptureProtection.h"
#include "../../ext/imgui/imgui.h"

namespace DesktopUi {
inline void (*createWindow)(ImGuiViewport*) = nullptr;
inline void (*showWindow)(ImGuiViewport*) = nullptr;
inline bool (*privacyEnabled)() = nullptr;
inline constexpr wchar_t affinityProperty[] = L"GoatDesktopAffinity";
inline ImGuiID menuId = 0;
inline bool* menuOpen = nullptr;
inline bool showTaskbar = false;
inline bool minimizedMenu = false;
inline bool closeRequested = false;

inline void Flags(ImGuiViewport* viewport) {
    viewport->Flags |= ImGuiViewportFlags_TopMost;
    viewport->Flags &= ~ImGuiViewportFlags_NoRendererClear;
    if (viewport->ID == menuId && showTaskbar)
        viewport->Flags &= ~ImGuiViewportFlags_NoTaskBarIcon;
    else viewport->Flags |= ImGuiViewportFlags_NoTaskBarIcon;
}

inline void PrepareMenu(bool taskbar) {
    showTaskbar = taskbar;
    ImGuiWindowClass windowClass;
    windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_TopMost;
    if (taskbar) windowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoTaskBarIcon;
    else windowClass.ViewportFlagsOverrideSet |= ImGuiViewportFlags_NoTaskBarIcon;
    ImGui::SetNextWindowClass(&windowClass);
}

inline void Menu(ImGuiViewport* viewport, bool& open) {
    menuId = viewport->ID;
    menuOpen = &open;
    if (viewport->PlatformRequestClose) {
        closeRequested = true;
        open = true;
    }
}

inline bool Protect(ImGuiViewport* viewport) {
    auto window = static_cast<HWND>(viewport->PlatformHandleRaw);
    if (!window) return false;
    const bool enabled = privacyEnabled && privacyEnabled();
    const auto marker = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(enabled ? 2 : 1));
    if (GetPropW(window, affinityProperty) == marker) return true;
    if (!CaptureProtection::Apply(window, enabled)) return false;
    SetPropW(window, affinityProperty, marker);
    return true;
}

inline void Create(ImGuiViewport* viewport) {
    Flags(viewport);
    createWindow(viewport);
    auto window = static_cast<HWND>(viewport->PlatformHandleRaw);
    if (HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1))) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }
    Protect(viewport);
}

inline void Show(ImGuiViewport* viewport) {
    if (Protect(viewport)) showWindow(viewport);
}

inline void Install(bool (*enabled)()) {
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigViewportsNoDefaultParent = true;
    io.ConfigViewportsNoTaskBarIcon = true;
    auto& platform = ImGui::GetPlatformIO();
    createWindow = platform.Platform_CreateWindow;
    showWindow = platform.Platform_ShowWindow;
    privacyEnabled = enabled;
    platform.Platform_CreateWindow = Create;
    platform.Platform_ShowWindow = Show;
}

inline void Update() {
    auto& platform = ImGui::GetPlatformIO();
    for (int i = 1; i < platform.Viewports.Size; ++i) {
        auto viewport = platform.Viewports[i];
        Flags(viewport);
        if (viewport->PlatformHandleRaw && !Protect(viewport))
            ShowWindow(static_cast<HWND>(viewport->PlatformHandleRaw), SW_HIDE);
    }
    ImGui::UpdatePlatformWindows();
    auto viewport = ImGui::FindViewportByID(menuId);
    if (viewport && viewport->PlatformHandleRaw && menuOpen) {
        auto window = static_cast<HWND>(viewport->PlatformHandleRaw);
        if (*menuOpen) {
            if (minimizedMenu) ShowWindow(window, SW_RESTORE);
            minimizedMenu = false;
        } else if (showTaskbar) {
            if (minimizedMenu && !IsIconic(window)) {
                *menuOpen = true;
                minimizedMenu = false;
            } else {
                if (!IsIconic(window)) ShowWindow(window, SW_MINIMIZE);
                minimizedMenu = true;
            }
        }
    } else minimizedMenu = false;
    ImGui::RenderPlatformWindowsDefault();
}

inline void RestoreGameFocus() {
    const HWND foreground = GetForegroundWindow();
    const auto& platform = ImGui::GetPlatformIO();
    for (int i = 1; i < platform.Viewports.Size; ++i) {
        if (platform.Viewports[i]->PlatformHandleRaw == foreground) {
            if (HWND game = FindWindowW(nullptr, L"Roblox")) SetForegroundWindow(game);
            return;
        }
    }
}

inline void PlaceCanvas(HWND canvas) {
    HWND last = nullptr;
    const auto& platform = ImGui::GetPlatformIO();
    int visited = 0;
    for (HWND window = GetTopWindow(nullptr); window && visited < 512;
         window = GetWindow(window, GW_HWNDNEXT), ++visited) {
        for (int i = 1; i < platform.Viewports.Size; ++i) {
            if (platform.Viewports[i]->PlatformHandleRaw == window &&
                IsWindowVisible(window) && !IsIconic(window)) last = window;
        }
    }
    if (last) SetWindowPos(canvas, last, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

template<class Draw>
inline void GameOverlay(ImDrawList* list, const ImVec2& size, Draw draw) {
    list->PushClipRect(ImVec2(0, 0), size, false);
    const int firstVertex = list->VtxBuffer.Size;
    const int firstCommand = list->CmdBuffer.Size - 1;
    draw();
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    for (int i = firstVertex; i < list->VtxBuffer.Size; ++i) {
        list->VtxBuffer[i].pos.x += origin.x;
        list->VtxBuffer[i].pos.y += origin.y;
    }
    for (int i = firstCommand; i < list->CmdBuffer.Size; ++i) {
        auto& clip = list->CmdBuffer[i].ClipRect;
        clip.x += origin.x; clip.z += origin.x;
        clip.y += origin.y; clip.w += origin.y;
    }
    list->PopClipRect();
}
}
