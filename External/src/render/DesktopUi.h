#pragma once
#include "CaptureProtection.h"
#include "MenuMotion.h"
#include "../../ext/imgui/UiPresentation.h"
#include "../../ext/imgui/imgui_internal.h"
#include "../core/variables/variables.h"
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
struct RevealState {
    HWND window=nullptr;
    int width=0,height=0,cell=0,phase=-2;
};
inline RevealState revealState;

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
    if (viewport->ID == menuId) revealState = {};
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

inline void Scale(){
    const float wanted=std::clamp(variables::Misc::menuScale,75.f,150.f)/100.f;
    static float pending=1;static ULONGLONG changed=0;const auto now=GetTickCount64();if(pending!=wanted){pending=wanted;changed=now;}
    if(wanted==UiPresentation::scale)return;
    if(GImGui->FrameCount>0&&(ImGui::GetIO().MouseDown[0]||GImGui->ActiveId||now-changed<180))return;
    const float ratio=UiPresentation::scale/wanted;UiPresentation::scale=wanted;
    auto& io=ImGui::GetIO();if(io.MousePos.x>-FLT_MAX/2){io.MousePos.x*=ratio;io.MousePos.y*=ratio;}
    for(auto& event:GImGui->InputEventsQueue)if(event.Type==ImGuiInputEventType_MousePos){event.MousePos.PosX*=ratio;event.MousePos.PosY*=ratio;}
    for(auto* window:GImGui->Windows)ImGui::SetWindowPos(window,ImVec2(window->Pos.x*ratio,window->Pos.y*ratio),ImGuiCond_Always);
    auto& p=ImGui::GetPlatformIO();
    for(int i=1;i<p.Viewports.Size;++i){auto* v=p.Viewports[i];v->Pos=ImVec2(v->Pos.x*ratio,v->Pos.y*ratio);if(v->PlatformHandleRaw){p.Platform_SetWindowPos(v,v->Pos);p.Platform_SetWindowSize(v,v->Size);if(v->RendererUserData)p.Renderer_SetWindowSize(v,v->Size);}}
}
inline void Reveal(ImGuiViewport* viewport){
    if(viewport->ID!=menuId||!viewport->PlatformHandleRaw)return;
    const auto window=static_cast<HWND>(viewport->PlatformHandleRaw);
    if(MenuMotion::alpha>=.999f){
        if(revealState.window==window&&revealState.phase!=-2&&SetWindowRgn(window,nullptr,FALSE))
            revealState={};
        return;
    }
    RECT bounds{};
    if(!GetClientRect(window,&bounds))return;
    const int cell=(std::max)(1,int(16*UiPresentation::scale));
    const int cols=(bounds.right+cell-1)/cell,rows=(bounds.bottom+cell-1)/cell;
    const int phase=MenuMotion::alpha<=0?-1:int(MenuMotion::alpha*(cols+rows-1));
    if(revealState.window==window&&revealState.width==bounds.right&&revealState.height==bounds.bottom&&
        revealState.cell==cell&&revealState.phase==phase)return;
    HRGN region=CreateRectRgn(0,0,0,0);
    if(!region)return;
    for(int row=0;row<rows;++row){
        const int count=std::clamp(phase-row+1,0,cols);
        if(!count)continue;
        HRGN strip=CreateRectRgn((std::max)(0,int(bounds.right)-count*cell),row*cell,
            bounds.right,(std::min)(int(bounds.bottom),(row+1)*cell));
        if(strip){CombineRgn(region,region,strip,RGN_OR);DeleteObject(strip);}
    }
    if(SetWindowRgn(window,region,FALSE))
        revealState={window,bounds.right,bounds.bottom,cell,phase};
    else DeleteObject(region);
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
    if (viewport) Reveal(viewport);
    if (viewport && viewport->PlatformHandleRaw && menuOpen) {
        auto window = static_cast<HWND>(viewport->PlatformHandleRaw);
        if (*menuOpen || MenuMotion::Visible()) {
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
        list->VtxBuffer[i].pos.x = list->VtxBuffer[i].pos.x/UiPresentation::scale + origin.x;
        list->VtxBuffer[i].pos.y = list->VtxBuffer[i].pos.y/UiPresentation::scale + origin.y;
    }
    for (int i = firstCommand; i < list->CmdBuffer.Size; ++i) {
        auto& clip = list->CmdBuffer[i].ClipRect;
        clip.x=clip.x/UiPresentation::scale+origin.x;clip.z=clip.z/UiPresentation::scale+origin.x;
        clip.y=clip.y/UiPresentation::scale+origin.y;clip.w=clip.w/UiPresentation::scale+origin.y;
    }
    list->PopClipRect();
}
}

