#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <dwmapi.h>
#include "../../src/render/WindowIdentity.h"

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include <xinput.h>
typedef DWORD(WINAPI* PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, XINPUT_STATE*);
#endif
typedef BOOL(WINAPI* PFN_AdjustWindowRectExForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static bool ImGui_ImplWin32_InitMultiViewportSupport(bool platform_has_own_dc);
static void ImGui_ImplWin32_ShutdownMultiViewportSupport();
static void ImGui_ImplWin32_UpdateMonitors();
static bool ImGui_ImplWin32_IsDpiAwarenessPerMonitorAwareV2();

struct ImGui_ImplWin32_Data
{
    HWND                        hWnd;
    wchar_t                     WindowClassName[64];
    HWND                        MouseHwnd;
    int                         MouseTrackedArea;
    int                         MouseButtonsDown;
    INT64                       Time;
    INT64                       TicksPerSecond;
    ImGuiMouseCursor            LastMouseCursor;
    UINT32                      KeyboardCodePage;
    bool                        WantUpdateMonitors;
    bool                        IsPerMonitorDpiAwareV2;
    PFN_AdjustWindowRectExForDpi AdjustWindowRectExForDpi;

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    bool                        HasGamepad;
    bool                        WantUpdateHasGamepad;
    HMODULE                     XInputDLL;
    PFN_XInputGetCapabilities   XInputGetCapabilities;
    PFN_XInputGetState          XInputGetState;
    DWORD                       XInputPacketNumber;
#endif

    ImGui_ImplWin32_Data()      { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplWin32_Data* ImGui_ImplWin32_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplWin32_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}
static ImGui_ImplWin32_Data* ImGui_ImplWin32_GetBackendData(ImGuiIO& io)
{
    return (ImGui_ImplWin32_Data*)io.BackendPlatformUserData;
}

static void ImGui_ImplWin32_UpdateKeyboardCodePage(ImGuiIO& io)
{

    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData(io);
    HKL keyboard_layout = ::GetKeyboardLayout(0);
    LCID keyboard_lcid = MAKELCID(HIWORD(keyboard_layout), SORT_DEFAULT);
    if (::GetLocaleInfoA(keyboard_lcid, (LOCALE_RETURN_NUMBER | LOCALE_IDEFAULTANSICODEPAGE), (LPSTR)&bd->KeyboardCodePage, sizeof(bd->KeyboardCodePage)) == 0)
        bd->KeyboardCodePage = CP_ACP;
}

static bool ImGui_ImplWin32_InitEx(void* hwnd, bool platform_has_own_dc)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    INT64 perf_frequency, perf_counter;
    if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER*)&perf_counter))
        return false;

    ImGui_ImplWin32_Data* bd = IM_NEW(ImGui_ImplWin32_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_win32";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    io.BackendFlags |= ImGuiBackendFlags_HasParentViewport;

    bd->hWnd = (HWND)hwnd;
    bd->TicksPerSecond = perf_frequency;
    bd->Time = perf_counter;
    bd->LastMouseCursor = ImGuiMouseCursor_COUNT;
    ImGui_ImplWin32_UpdateKeyboardCodePage(io);

    ImGui_ImplWin32_UpdateMonitors();

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = (void*)bd->hWnd;

    if (!ImGui_ImplWin32_InitMultiViewportSupport(platform_has_own_dc))
    {
        main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = nullptr;
        io.BackendPlatformName = nullptr;
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport | ImGuiBackendFlags_HasParentViewport);
        IM_DELETE(bd);
        return false;
    }
    ::SetPropA(bd->hWnd, "IMGUI_CONTEXT", ImGui::GetCurrentContext());

    if (HINSTANCE user32_dll = ::GetModuleHandleA("user32.dll"))
        bd->AdjustWindowRectExForDpi = (PFN_AdjustWindowRectExForDpi)::GetProcAddress(user32_dll, "AdjustWindowRectExForDpi");
    bd->IsPerMonitorDpiAwareV2 = ImGui_ImplWin32_IsDpiAwarenessPerMonitorAwareV2();

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    bd->WantUpdateHasGamepad = true;
    const char* xinput_dll_names[] =
    {
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll",
        "xinput1_2.dll",
        "xinput1_1.dll"
    };
    for (int n = 0; n < IM_COUNTOF(xinput_dll_names); n++)
        if (HMODULE dll = ::LoadLibraryA(xinput_dll_names[n]))
        {
            bd->XInputDLL = dll;
            bd->XInputGetCapabilities = (PFN_XInputGetCapabilities)::GetProcAddress(dll, "XInputGetCapabilities");
            bd->XInputGetState = (PFN_XInputGetState)::GetProcAddress(dll, "XInputGetState");
            break;
        }
#endif

    return true;
}

IMGUI_IMPL_API bool     ImGui_ImplWin32_Init(void* hwnd)
{
    return ImGui_ImplWin32_InitEx(hwnd, false);
}

IMGUI_IMPL_API bool     ImGui_ImplWin32_InitForOpenGL(void* hwnd)
{

    return ImGui_ImplWin32_InitEx(hwnd, true);
}

void    ImGui_ImplWin32_Shutdown()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ::SetPropA(bd->hWnd, "IMGUI_CONTEXT", nullptr);
    ImGui_ImplWin32_ShutdownMultiViewportSupport();

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    if (bd->XInputDLL)
        ::FreeLibrary(bd->XInputDLL);
#endif

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad | ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasMouseHoveredViewport | ImGuiBackendFlags_HasParentViewport);
    platform_io.ClearPlatformHandlers();
    IM_DELETE(bd);
}

static bool ImGui_ImplWin32_UpdateMouseCursor(ImGuiIO& io, ImGuiMouseCursor imgui_cursor)
{
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {

        ::SetCursor(nullptr);
    }
    else
    {

        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
        case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
        case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
        case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
        case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
        case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
        case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
        case ImGuiMouseCursor_Hand:         win32_cursor = IDC_HAND; break;
        case ImGuiMouseCursor_Wait:         win32_cursor = IDC_WAIT; break;
        case ImGuiMouseCursor_Progress:     win32_cursor = IDC_APPSTARTING; break;
        case ImGuiMouseCursor_NotAllowed:   win32_cursor = IDC_NO; break;
        }
        ::SetCursor(::LoadCursor(nullptr, win32_cursor));
    }
    return true;
}

static bool IsVkDown(int vk)
{
    return (::GetKeyState(vk) & 0x8000) != 0;
}

static void ImGui_ImplWin32_AddKeyEvent(ImGuiIO& io, ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
{
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode);
    IM_UNUSED(native_scancode);
}

static void ImGui_ImplWin32_ProcessKeyEventsWorkarounds(ImGuiIO& io)
{

    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && !IsVkDown(VK_LSHIFT))
        ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftShift, false, VK_LSHIFT);
    if (ImGui::IsKeyDown(ImGuiKey_RightShift) && !IsVkDown(VK_RSHIFT))
        ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightShift, false, VK_RSHIFT);

    if (ImGui::IsKeyDown(ImGuiKey_LeftSuper) && !IsVkDown(VK_LWIN))
        ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftSuper, false, VK_LWIN);
    if (ImGui::IsKeyDown(ImGuiKey_RightSuper) && !IsVkDown(VK_RWIN))
        ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightSuper, false, VK_RWIN);
}

static void ImGui_ImplWin32_UpdateKeyModifiers(ImGuiIO& io)
{
    io.AddKeyEvent(ImGuiMod_Ctrl, IsVkDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, IsVkDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, IsVkDown(VK_MENU));
    io.AddKeyEvent(ImGuiMod_Super, IsVkDown(VK_LWIN) || IsVkDown(VK_RWIN));
}

static ImGuiViewport* ImGui_ImplWin32_FindViewportByPlatformHandle(ImGuiPlatformIO& platform_io, HWND hwnd)
{

    for (ImGuiViewport* viewport : platform_io.Viewports)
        if (viewport->PlatformHandle == hwnd)
            return viewport;
    return nullptr;
}

static void ImGui_ImplWin32_UpdateMouseData(ImGuiIO& io, ImGuiPlatformIO& platform_io)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData(io);
    IM_ASSERT(bd->hWnd != 0);

    POINT mouse_screen_pos;
    bool has_mouse_screen_pos = ::GetCursorPos(&mouse_screen_pos) != 0;

    HWND focused_window = ::GetForegroundWindow();
    const bool is_app_focused = (focused_window && (focused_window == bd->hWnd || ::IsChild(focused_window, bd->hWnd) || ImGui_ImplWin32_FindViewportByPlatformHandle(platform_io, focused_window)));
    if (is_app_focused)
    {

        if (io.WantSetMousePos)
        {
            POINT pos = { (int)io.MousePos.x, (int)io.MousePos.y };
            if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
                ::ClientToScreen(focused_window, &pos);
            ::SetCursorPos(pos.x, pos.y);
        }

        if (!io.WantSetMousePos && bd->MouseTrackedArea == 0 && has_mouse_screen_pos)
        {

            POINT mouse_pos = mouse_screen_pos;
            if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
                ::ScreenToClient(bd->hWnd, &mouse_pos);
            io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
        }
    }

    ImGuiID mouse_viewport_id = 0;
    if (has_mouse_screen_pos)
        if (HWND hovered_hwnd = ::WindowFromPoint(mouse_screen_pos))
            if (ImGuiViewport* viewport = ImGui_ImplWin32_FindViewportByPlatformHandle(platform_io, hovered_hwnd))
                mouse_viewport_id = viewport->ID;
    io.AddMouseViewportEvent(mouse_viewport_id);
}

static void ImGui_ImplWin32_UpdateGamepads(ImGuiIO& io)
{
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData(io);

    if (bd->WantUpdateHasGamepad)
    {
        XINPUT_CAPABILITIES caps = {};
        bd->HasGamepad = bd->XInputGetCapabilities ? (bd->XInputGetCapabilities(0, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS) : false;
        bd->WantUpdateHasGamepad = false;
    }

    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    XINPUT_STATE xinput_state;
    XINPUT_GAMEPAD& gamepad = xinput_state.Gamepad;
    if (!bd->HasGamepad || bd->XInputGetState == nullptr || bd->XInputGetState(0, &xinput_state) != ERROR_SUCCESS)
        return;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    if (bd->XInputPacketNumber != 0 && bd->XInputPacketNumber == xinput_state.dwPacketNumber)
        return;
    bd->XInputPacketNumber = xinput_state.dwPacketNumber;

    #define IM_SATURATE(V)                      (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
    #define MAP_BUTTON(KEY_NO, BUTTON_ENUM)     { io.AddKeyEvent(KEY_NO, (gamepad.wButtons & BUTTON_ENUM) != 0); }
    #define MAP_ANALOG(KEY_NO, VALUE, V0, V1)   { float vn = (float)(VALUE - V0) / (float)(V1 - V0); io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); }
    MAP_BUTTON(ImGuiKey_GamepadStart,           XINPUT_GAMEPAD_START);
    MAP_BUTTON(ImGuiKey_GamepadBack,            XINPUT_GAMEPAD_BACK);
    MAP_BUTTON(ImGuiKey_GamepadFaceLeft,        XINPUT_GAMEPAD_X);
    MAP_BUTTON(ImGuiKey_GamepadFaceRight,       XINPUT_GAMEPAD_B);
    MAP_BUTTON(ImGuiKey_GamepadFaceUp,          XINPUT_GAMEPAD_Y);
    MAP_BUTTON(ImGuiKey_GamepadFaceDown,        XINPUT_GAMEPAD_A);
    MAP_BUTTON(ImGuiKey_GamepadDpadLeft,        XINPUT_GAMEPAD_DPAD_LEFT);
    MAP_BUTTON(ImGuiKey_GamepadDpadRight,       XINPUT_GAMEPAD_DPAD_RIGHT);
    MAP_BUTTON(ImGuiKey_GamepadDpadUp,          XINPUT_GAMEPAD_DPAD_UP);
    MAP_BUTTON(ImGuiKey_GamepadDpadDown,        XINPUT_GAMEPAD_DPAD_DOWN);
    MAP_BUTTON(ImGuiKey_GamepadL1,              XINPUT_GAMEPAD_LEFT_SHOULDER);
    MAP_BUTTON(ImGuiKey_GamepadR1,              XINPUT_GAMEPAD_RIGHT_SHOULDER);
    MAP_ANALOG(ImGuiKey_GamepadL2,              gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_ANALOG(ImGuiKey_GamepadR2,              gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    MAP_BUTTON(ImGuiKey_GamepadL3,              XINPUT_GAMEPAD_LEFT_THUMB);
    MAP_BUTTON(ImGuiKey_GamepadR3,              XINPUT_GAMEPAD_RIGHT_THUMB);
    MAP_ANALOG(ImGuiKey_GamepadLStickLeft,      gamepad.sThumbLX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickRight,     gamepad.sThumbLX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickUp,        gamepad.sThumbLY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickDown,      gamepad.sThumbLY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickLeft,      gamepad.sThumbRX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickRight,     gamepad.sThumbRX, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickUp,        gamepad.sThumbRY, +XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickDown,      gamepad.sThumbRY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    #undef MAP_BUTTON
    #undef MAP_ANALOG
#else
    IM_UNUSED(io);
#endif
}

static BOOL CALLBACK ImGui_ImplWin32_UpdateMonitors_EnumFunc(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    if (!::GetMonitorInfo(monitor, &info))
        return TRUE;
    ImGuiPlatformMonitor imgui_monitor;
    imgui_monitor.MainPos = ImVec2((float)info.rcMonitor.left, (float)info.rcMonitor.top);
    imgui_monitor.MainSize = ImVec2((float)(info.rcMonitor.right - info.rcMonitor.left), (float)(info.rcMonitor.bottom - info.rcMonitor.top));
    imgui_monitor.WorkPos = ImVec2((float)info.rcWork.left, (float)info.rcWork.top);
    imgui_monitor.WorkSize = ImVec2((float)(info.rcWork.right - info.rcWork.left), (float)(info.rcWork.bottom - info.rcWork.top));
    imgui_monitor.DpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    imgui_monitor.PlatformHandle = (void*)monitor;
    if (imgui_monitor.DpiScale <= 0.0f)
        return TRUE;
    ImGuiPlatformIO& io = ImGui::GetPlatformIO();
    if (info.dwFlags & MONITORINFOF_PRIMARY)
        io.Monitors.push_front(imgui_monitor);
    else
        io.Monitors.push_back(imgui_monitor);
    return TRUE;
}

static void ImGui_ImplWin32_UpdateMonitors()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    ImGui::GetPlatformIO().Monitors.resize(0);
    ::EnumDisplayMonitors(nullptr, nullptr, ImGui_ImplWin32_UpdateMonitors_EnumFunc, 0);
    bd->WantUpdateMonitors = false;
}

void    ImGui_ImplWin32_NewFrame()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized? Did you call ImGui_ImplWin32_Init()?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    RECT rect = { 0, 0, 0, 0 };
    ::GetClientRect(bd->hWnd, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
    if (bd->WantUpdateMonitors)
        ImGui_ImplWin32_UpdateMonitors();

    INT64 current_time = 0;
    ::QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - bd->Time) / bd->TicksPerSecond;
    bd->Time = current_time;

    ImGui_ImplWin32_UpdateMouseData(io, platform_io);

    ImGui_ImplWin32_ProcessKeyEventsWorkarounds(io);

    ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (bd->LastMouseCursor != mouse_cursor)
    {
        bd->LastMouseCursor = mouse_cursor;
        ImGui_ImplWin32_UpdateMouseCursor(io, mouse_cursor);
    }

    ImGui_ImplWin32_UpdateGamepads(io);
}

ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam);
ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam)
{

    if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
        return ImGuiKey_KeypadEnter;

    const int scancode = (int)LOBYTE(HIWORD(lParam));

    switch (wParam)
    {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_PRIOR: return ImGuiKey_PageUp;
        case VK_NEXT: return ImGuiKey_PageDown;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_INSERT: return ImGuiKey_Insert;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_SPACE: return ImGuiKey_Space;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;

        case VK_OEM_COMMA: return ImGuiKey_Comma;

        case VK_OEM_PERIOD: return ImGuiKey_Period;

        case VK_CAPITAL: return ImGuiKey_CapsLock;
        case VK_SCROLL: return ImGuiKey_ScrollLock;
        case VK_NUMLOCK: return ImGuiKey_NumLock;
        case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
        case VK_PAUSE: return ImGuiKey_Pause;
        case VK_NUMPAD0: return ImGuiKey_Keypad0;
        case VK_NUMPAD1: return ImGuiKey_Keypad1;
        case VK_NUMPAD2: return ImGuiKey_Keypad2;
        case VK_NUMPAD3: return ImGuiKey_Keypad3;
        case VK_NUMPAD4: return ImGuiKey_Keypad4;
        case VK_NUMPAD5: return ImGuiKey_Keypad5;
        case VK_NUMPAD6: return ImGuiKey_Keypad6;
        case VK_NUMPAD7: return ImGuiKey_Keypad7;
        case VK_NUMPAD8: return ImGuiKey_Keypad8;
        case VK_NUMPAD9: return ImGuiKey_Keypad9;
        case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE: return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case VK_ADD: return ImGuiKey_KeypadAdd;
        case VK_LSHIFT: return ImGuiKey_LeftShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_LMENU: return ImGuiKey_LeftAlt;
        case VK_LWIN: return ImGuiKey_LeftSuper;
        case VK_RSHIFT: return ImGuiKey_RightShift;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_RMENU: return ImGuiKey_RightAlt;
        case VK_RWIN: return ImGuiKey_RightSuper;
        case VK_APPS: return ImGuiKey_Menu;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        case VK_F1: return ImGuiKey_F1;
        case VK_F2: return ImGuiKey_F2;
        case VK_F3: return ImGuiKey_F3;
        case VK_F4: return ImGuiKey_F4;
        case VK_F5: return ImGuiKey_F5;
        case VK_F6: return ImGuiKey_F6;
        case VK_F7: return ImGuiKey_F7;
        case VK_F8: return ImGuiKey_F8;
        case VK_F9: return ImGuiKey_F9;
        case VK_F10: return ImGuiKey_F10;
        case VK_F11: return ImGuiKey_F11;
        case VK_F12: return ImGuiKey_F12;
        case VK_F13: return ImGuiKey_F13;
        case VK_F14: return ImGuiKey_F14;
        case VK_F15: return ImGuiKey_F15;
        case VK_F16: return ImGuiKey_F16;
        case VK_F17: return ImGuiKey_F17;
        case VK_F18: return ImGuiKey_F18;
        case VK_F19: return ImGuiKey_F19;
        case VK_F20: return ImGuiKey_F20;
        case VK_F21: return ImGuiKey_F21;
        case VK_F22: return ImGuiKey_F22;
        case VK_F23: return ImGuiKey_F23;
        case VK_F24: return ImGuiKey_F24;
        case VK_BROWSER_BACK: return ImGuiKey_AppBack;
        case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }

    switch (scancode)
    {
    case 41: return ImGuiKey_GraveAccent;
    case 12: return ImGuiKey_Minus;
    case 13: return ImGuiKey_Equal;
    case 26: return ImGuiKey_LeftBracket;
    case 27: return ImGuiKey_RightBracket;
    case 86: return ImGuiKey_Oem102;
    case 43: return ImGuiKey_Backslash;
    case 39: return ImGuiKey_Semicolon;
    case 40: return ImGuiKey_Apostrophe;
    case 51: return ImGuiKey_Comma;
    case 52: return ImGuiKey_Period;
    case 53: return ImGuiKey_Slash;
    default: break;
    }

    return ImGuiKey_None;
}

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif

static ImGuiMouseSource ImGui_ImplWin32_GetMouseSourceFromMessageExtraInfo()
{
    LPARAM extra_info = ::GetMessageExtraInfo();
    if ((extra_info & 0xFFFFFF80) == 0xFF515700)
        return ImGuiMouseSource_Pen;
    if ((extra_info & 0xFFFFFF80) == 0xFF515780)
        return ImGuiMouseSource_TouchScreen;
    return ImGuiMouseSource_Mouse;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ImGuiIO& io);

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    if (ImGui::GetCurrentContext() == nullptr)
        return 0;
    return ImGui_ImplWin32_WndProcHandlerEx(hwnd, msg, wParam, lParam, ImGui::GetIO());
}

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, ImGuiIO& io)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData(io);
    if (bd == nullptr)
        return 0;
    switch (msg)
    {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    {

        ImGuiMouseSource mouse_source = ImGui_ImplWin32_GetMouseSourceFromMessageExtraInfo();
        const int area = (msg == WM_MOUSEMOVE) ? 1 : 2;
        bd->MouseHwnd = hwnd;
        if (bd->MouseTrackedArea != area)
        {
            TRACKMOUSEEVENT tme_cancel = { sizeof(tme_cancel), TME_CANCEL, hwnd, 0 };
            TRACKMOUSEEVENT tme_track = { sizeof(tme_track), (DWORD)((area == 2) ? (TME_LEAVE | TME_NONCLIENT) : TME_LEAVE), hwnd, 0 };
            if (bd->MouseTrackedArea != 0)
                ::TrackMouseEvent(&tme_cancel);
            ::TrackMouseEvent(&tme_track);
            bd->MouseTrackedArea = area;
        }
        POINT mouse_pos = { (LONG)GET_X_LPARAM(lParam), (LONG)GET_Y_LPARAM(lParam) };
        bool want_absolute_pos = (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
        if (msg == WM_MOUSEMOVE && want_absolute_pos)
            ::ClientToScreen(hwnd, &mouse_pos);
        if (msg == WM_NCMOUSEMOVE && !want_absolute_pos)
            ::ScreenToClient(hwnd, &mouse_pos);
        io.AddMouseSourceEvent(mouse_source);
        io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
        return 0;
    }
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
    {
        const int area = (msg == WM_MOUSELEAVE) ? 1 : 2;
        if (bd->MouseTrackedArea == area)
        {
            if (bd->MouseHwnd == hwnd)
                bd->MouseHwnd = nullptr;
            bd->MouseTrackedArea = 0;
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
        return 0;
    }
    case WM_DESTROY:
        if (bd->MouseHwnd == hwnd && bd->MouseTrackedArea != 0)
        {
            TRACKMOUSEEVENT tme_cancel = { sizeof(tme_cancel), TME_CANCEL, hwnd, 0 };
            ::TrackMouseEvent(&tme_cancel);
            bd->MouseHwnd = nullptr;
            bd->MouseTrackedArea = 0;
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
        return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
    {
        ImGuiMouseSource mouse_source = ImGui_ImplWin32_GetMouseSourceFromMessageExtraInfo();
        int button = 0;
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
        if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        HWND hwnd_with_capture = ::GetCapture();
        if (bd->MouseButtonsDown != 0 && hwnd_with_capture != hwnd)
            bd->MouseButtonsDown = 0;
        if (bd->MouseButtonsDown == 0 && hwnd_with_capture == nullptr)
            ::SetCapture(hwnd);
        bd->MouseButtonsDown |= 1 << button;
        io.AddMouseSourceEvent(mouse_source);
        io.AddMouseButtonEvent(button, true);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    {
        ImGuiMouseSource mouse_source = ImGui_ImplWin32_GetMouseSourceFromMessageExtraInfo();
        int button = 0;
        if (msg == WM_LBUTTONUP) { button = 0; }
        if (msg == WM_RBUTTONUP) { button = 1; }
        if (msg == WM_MBUTTONUP) { button = 2; }
        if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        bd->MouseButtonsDown &= ~(1 << button);
        if (bd->MouseButtonsDown == 0 && ::GetCapture() == hwnd)
            ::ReleaseCapture();
        io.AddMouseSourceEvent(mouse_source);
        io.AddMouseButtonEvent(button, false);
        return 0;
    }
    case WM_MOUSEWHEEL:
        io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
        return 0;
    case WM_MOUSEHWHEEL:
        io.AddMouseWheelEvent(-(float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        const bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        if (wParam < 256)
        {

            ImGui_ImplWin32_UpdateKeyModifiers(io);

            const ImGuiKey key = ImGui_ImplWin32_KeyEventToImGuiKey(wParam, lParam);
            const int vk = (int)wParam;
            const int scancode = (int)LOBYTE(HIWORD(lParam));

            if (key == ImGuiKey_PrintScreen && !is_key_down)
                ImGui_ImplWin32_AddKeyEvent(io, key, true, vk, scancode);

            if (key != ImGuiKey_None)
                ImGui_ImplWin32_AddKeyEvent(io, key, is_key_down, vk, scancode);

            if (vk == VK_SHIFT)
            {

                if (IsVkDown(VK_LSHIFT) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftShift, is_key_down, VK_LSHIFT, scancode); }
                if (IsVkDown(VK_RSHIFT) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightShift, is_key_down, VK_RSHIFT, scancode); }
            }
            else if (vk == VK_CONTROL)
            {
                if (IsVkDown(VK_LCONTROL) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftCtrl, is_key_down, VK_LCONTROL, scancode); }
                if (IsVkDown(VK_RCONTROL) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightCtrl, is_key_down, VK_RCONTROL, scancode); }
            }
            else if (vk == VK_MENU)
            {
                if (IsVkDown(VK_LMENU) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftAlt, is_key_down, VK_LMENU, scancode); }
                if (IsVkDown(VK_RMENU) == is_key_down) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightAlt, is_key_down, VK_RMENU, scancode); }
            }
        }
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        io.AddFocusEvent(msg == WM_SETFOCUS);
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
        bd->XInputPacketNumber = 0;
#endif
        return 0;
    case WM_INPUTLANGCHANGE:
        ImGui_ImplWin32_UpdateKeyboardCodePage(io);
        return 0;
    case WM_CHAR:
        if (::IsWindowUnicode(hwnd))
        {

            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((unsigned short)wParam);
        }
        else
        {
            wchar_t wch = 0;
            ::MultiByteToWideChar(bd->KeyboardCodePage, MB_PRECOMPOSED, (char*)&wParam, 2, &wch, 1);
            io.AddInputCharacter(wch);
        }
        return 0;
    case WM_IME_COMPOSITION:
    {

        LRESULT result = ::DefWindowProcW(hwnd, msg, wParam, lParam);
        return (lParam & GCS_RESULTSTR) ? 1 : result;
    }
    case WM_IME_CHAR:
        if (::IsWindowUnicode(hwnd) == FALSE)
        {
            if (::IsDBCSLeadByte(HIBYTE(wParam)))
                wParam = (WPARAM)MAKEWORD(HIBYTE(wParam), LOBYTE(wParam));
            wchar_t wch = 0;
            ::MultiByteToWideChar(bd->KeyboardCodePage, MB_PRECOMPOSED, (char*)&wParam, 2, &wch, 1);
            io.AddInputCharacterUTF16(wch);
            return 1;
        }
        return 0;
    case WM_SETCURSOR:

        if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor(io, bd->LastMouseCursor))
            return 1;
        return 0;
    case WM_DEVICECHANGE:
#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
        if ((UINT)wParam == DBT_DEVNODES_CHANGED)
            bd->WantUpdateHasGamepad = true;
#endif
        return 0;
    case WM_DISPLAYCHANGE:
        bd->WantUpdateMonitors = true;
        return 0;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA)
            bd->WantUpdateMonitors = true;
        return 0;
    case WM_DPICHANGED:
    {

        const RECT* suggested_rect = (RECT*)lParam;
        if (io.ConfigDpiScaleViewports)
            ::SetWindowPos(hwnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return 0;
}

static BOOL _IsWindowsVersionOrGreater(WORD major, WORD minor, WORD)
{
    typedef LONG(WINAPI* PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW*, ULONG, ULONGLONG);
    static PFN_RtlVerifyVersionInfo RtlVerifyVersionInfoFn = nullptr;
    if (RtlVerifyVersionInfoFn == nullptr)
        if (HMODULE ntdllModule = ::GetModuleHandleA("ntdll.dll"))
            RtlVerifyVersionInfoFn = (PFN_RtlVerifyVersionInfo)GetProcAddress(ntdllModule, "RtlVerifyVersionInfo");
    if (RtlVerifyVersionInfoFn == nullptr)
        return FALSE;

    RTL_OSVERSIONINFOEXW versionInfo = { };
    ULONGLONG conditionMask = 0;
    versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
    versionInfo.dwMajorVersion = major;
    versionInfo.dwMinorVersion = minor;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    return (RtlVerifyVersionInfoFn(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) == 0) ? TRUE : FALSE;
}

#define _IsWindowsVistaOrGreater()   _IsWindowsVersionOrGreater(HIBYTE(0x0600), LOBYTE(0x0600), 0)
#define _IsWindows8OrGreater()       _IsWindowsVersionOrGreater(HIBYTE(0x0602), LOBYTE(0x0602), 0)
#define _IsWindows8Point1OrGreater() _IsWindowsVersionOrGreater(HIBYTE(0x0603), LOBYTE(0x0603), 0)
#define _IsWindows10OrGreater()      _IsWindowsVersionOrGreater(HIBYTE(0x0A00), LOBYTE(0x0A00), 0)

#ifndef DPI_ENUMS_DECLARED
typedef enum { PROCESS_DPI_UNAWARE = 0, PROCESS_SYSTEM_DPI_AWARE = 1, PROCESS_PER_MONITOR_DPI_AWARE = 2 } PROCESS_DPI_AWARENESS;
typedef enum { MDT_EFFECTIVE_DPI = 0, MDT_ANGULAR_DPI = 1, MDT_RAW_DPI = 2, MDT_DEFAULT = MDT_EFFECTIVE_DPI } MONITOR_DPI_TYPE;
#endif
#ifndef _DPI_AWARENESS_CONTEXTS_
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE    (DPI_AWARENESS_CONTEXT)-3
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (DPI_AWARENESS_CONTEXT)-4
#endif
typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
typedef HRESULT(WINAPI* PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
typedef DPI_AWARENESS_CONTEXT(WINAPI* PFN_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
typedef DPI_AWARENESS_CONTEXT(WINAPI* PFN_GetThreadDpiAwarenessContext)();
typedef BOOL(WINAPI* PFN_AreDpiAwarenessContextsEqual)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);

void ImGui_ImplWin32_EnableDpiAwareness()
{

    if (ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData())
        bd->WantUpdateMonitors = true;

    if (_IsWindows10OrGreater())
    {
        static HINSTANCE user32_dll = ::LoadLibraryA("user32.dll");
        if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn = (PFN_SetThreadDpiAwarenessContext)::GetProcAddress(user32_dll, "SetThreadDpiAwarenessContext"))
        {
            SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    if (_IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll");
        if (PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn = (PFN_SetProcessDpiAwareness)::GetProcAddress(shcore_dll, "SetProcessDpiAwareness"))
        {
            SetProcessDpiAwarenessFn(PROCESS_PER_MONITOR_DPI_AWARE);
            return;
        }
    }
#if _WIN32_WINNT >= 0x0600
    ::SetProcessDPIAware();
#endif
}

static bool ImGui_ImplWin32_IsDpiAwarenessPerMonitorAwareV2()
{
    if (!_IsWindows10OrGreater())
        return false;
    static HINSTANCE user32_dll = ::GetModuleHandleA("user32.dll");
    PFN_GetThreadDpiAwarenessContext GetThreadDpiAwarenessContextFn = (PFN_GetThreadDpiAwarenessContext)::GetProcAddress(user32_dll, "GetThreadDpiAwarenessContext");
    PFN_AreDpiAwarenessContextsEqual AreDpiAwarenessContextsEqualFn = (PFN_AreDpiAwarenessContextsEqual)::GetProcAddress(user32_dll, "AreDpiAwarenessContextsEqual");
    if (GetThreadDpiAwarenessContextFn && AreDpiAwarenessContextsEqualFn)
        return AreDpiAwarenessContextsEqualFn(GetThreadDpiAwarenessContextFn(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != 0;
    return false;
}

#if defined(_MSC_VER) && !defined(NOGDI)
#pragma comment(lib, "gdi32")
#endif

float ImGui_ImplWin32_GetDpiScaleForMonitor(void* monitor)
{
    UINT xdpi = 96, ydpi = 96;
    if (_IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll");
        static PFN_GetDpiForMonitor GetDpiForMonitorFn = nullptr;
        if (GetDpiForMonitorFn == nullptr && shcore_dll != nullptr)
            GetDpiForMonitorFn = (PFN_GetDpiForMonitor)::GetProcAddress(shcore_dll, "GetDpiForMonitor");
        if (GetDpiForMonitorFn != nullptr)
        {
            GetDpiForMonitorFn((HMONITOR)monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
            IM_ASSERT(xdpi == ydpi);
            return xdpi / 96.0f;
        }
    }
#ifndef NOGDI
    const HDC dc = ::GetDC(nullptr);
    xdpi = ::GetDeviceCaps(dc, LOGPIXELSX);
    ydpi = ::GetDeviceCaps(dc, LOGPIXELSY);
    IM_ASSERT(xdpi == ydpi);
    ::ReleaseDC(nullptr, dc);
#endif
    return xdpi / 96.0f;
}

float ImGui_ImplWin32_GetDpiScaleForHwnd(void* hwnd)
{
    HMONITOR monitor = ::MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
    return ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
}

#if defined(_MSC_VER)
#pragma comment(lib, "dwmapi")
#endif

void ImGui_ImplWin32_EnableAlphaCompositing(void* hwnd)
{
    if (!_IsWindowsVistaOrGreater())
        return;

    BOOL composition;
    if (FAILED(::DwmIsCompositionEnabled(&composition)) || !composition)
        return;

    BOOL opaque;
    DWORD color;
    if (_IsWindows8OrGreater() || (SUCCEEDED(::DwmGetColorizationColor(&color, &opaque)) && !opaque))
    {
        HRGN region = ::CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.hRgnBlur = region;
        bb.fEnable = TRUE;
        ::DwmEnableBlurBehindWindow((HWND)hwnd, &bb);
        ::DeleteObject(region);
    }
    else
    {
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        ::DwmEnableBlurBehindWindow((HWND)hwnd, &bb);
    }
}

struct ImGui_ImplWin32_ViewportData
{
    HWND    Hwnd;
    HWND    HwndParent;
    bool    HwndOwned;
    DWORD   DwStyle;
    DWORD   DwExStyle;

    ImGui_ImplWin32_ViewportData() { Hwnd = HwndParent = nullptr; HwndOwned = false;  DwStyle = DwExStyle = 0; }
    ~ImGui_ImplWin32_ViewportData() { IM_ASSERT(Hwnd == nullptr); }
};

static void ImGui_ImplWin32_GetWin32StyleFromViewportFlags(ImGuiViewportFlags flags, DWORD* out_style, DWORD* out_ex_style)
{
    if (flags & ImGuiViewportFlags_NoDecoration)
        *out_style = WS_POPUP;
    else
        *out_style = WS_OVERLAPPEDWINDOW;

    if (flags & ImGuiViewportFlags_NoTaskBarIcon)
        *out_ex_style = WS_EX_TOOLWINDOW;
    else
        *out_ex_style = WS_EX_APPWINDOW;

    if (flags & ImGuiViewportFlags_TopMost)
        *out_ex_style |= WS_EX_TOPMOST;
}

static HWND ImGui_ImplWin32_GetHwndFromViewport(ImGuiViewport* viewport)
{
    if (viewport != nullptr)
        return (HWND)viewport->PlatformHandle;
    return nullptr;
}

static void ImGui_ImplWin32_AdjustWindowRect(ImGuiViewport* viewport, RECT* rect, DWORD style, DWORD ex_style)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    if (bd->AdjustWindowRectExForDpi && bd->IsPerMonitorDpiAwareV2)
    {
        UINT dpi = (UINT)(viewport->DpiScale * 96.0f + 0.5f);
        if (dpi != 0 && bd->AdjustWindowRectExForDpi(rect, style, FALSE, ex_style, dpi))
            return;
    }
    ::AdjustWindowRectEx(rect, style, FALSE, ex_style);
}

static void ImGui_ImplWin32_CreateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    viewport->PlatformUserData = vd;

    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &vd->DwStyle, &vd->DwExStyle);
    vd->HwndParent = ImGui_ImplWin32_GetHwndFromViewport(viewport->ParentViewport);

    RECT rect = { (LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x), (LONG)(viewport->Pos.y + viewport->Size.y) };
    ImGui_ImplWin32_AdjustWindowRect(viewport, &rect, vd->DwStyle, vd->DwExStyle);
    vd->Hwnd = ::CreateWindowExW(
        vd->DwExStyle, ImGui_ImplWin32_GetBackendData()->WindowClassName, L"Untitled", vd->DwStyle,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        vd->HwndParent, nullptr, ::GetModuleHandle(nullptr), nullptr);
    vd->HwndOwned = true;
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = viewport->PlatformHandleRaw = vd->Hwnd;

    ::SetPropA(vd->Hwnd, "IMGUI_CONTEXT", ImGui::GetCurrentContext());
}

static void ImGui_ImplWin32_DestroyWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    if (ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData)
    {
        if (::GetCapture() == vd->Hwnd)
        {

            ::ReleaseCapture();
            ::SetCapture(bd->hWnd);
        }
        if (vd->Hwnd && vd->HwndOwned)
            ::DestroyWindow(vd->Hwnd);
        vd->Hwnd = nullptr;
        IM_DELETE(vd);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplWin32_ShowWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);

    bool avoid_bringing_parent_to_front = vd->HwndParent != NULL && (viewport->Flags & (ImGuiViewportFlags_NoFocusOnAppearing | ImGuiViewportFlags_NoTaskBarIcon)) != 0;
    if (avoid_bringing_parent_to_front)
        ::SetWindowLongPtr(vd->Hwnd, GWLP_HWNDPARENT, (LONG_PTR)nullptr);

    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
        ::ShowWindow(vd->Hwnd, SW_SHOWNA);
    else
        ::ShowWindow(vd->Hwnd, SW_SHOW);

    if (avoid_bringing_parent_to_front)
        ::SetWindowLongPtr(vd->Hwnd, GWLP_HWNDPARENT, (LONG_PTR)vd->HwndParent);
}

static void ImGui_ImplWin32_UpdateWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);

    HWND new_parent = ImGui_ImplWin32_GetHwndFromViewport(viewport->ParentViewport);
    if (new_parent != vd->HwndParent)
    {

        vd->HwndParent = new_parent;
        ::SetWindowLongPtr(vd->Hwnd, GWLP_HWNDPARENT, (LONG_PTR)vd->HwndParent);
    }

    DWORD new_style;
    DWORD new_ex_style;
    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &new_style, &new_ex_style);

    if (vd->DwStyle != new_style || vd->DwExStyle != new_ex_style)
    {

        bool top_most_changed = (vd->DwExStyle & WS_EX_TOPMOST) != (new_ex_style & WS_EX_TOPMOST);
        HWND insert_after = top_most_changed ? ((viewport->Flags & ImGuiViewportFlags_TopMost) ? HWND_TOPMOST : HWND_NOTOPMOST) : 0;
        UINT swp_flag = top_most_changed ? 0 : SWP_NOZORDER;

        vd->DwStyle = new_style;
        vd->DwExStyle = new_ex_style;
        ::SetWindowLong(vd->Hwnd, GWL_STYLE, vd->DwStyle);
        ::SetWindowLong(vd->Hwnd, GWL_EXSTYLE, vd->DwExStyle);
        RECT rect = { (LONG)viewport->Pos.x, (LONG)viewport->Pos.y, (LONG)(viewport->Pos.x + viewport->Size.x), (LONG)(viewport->Pos.y + viewport->Size.y) };
        ImGui_ImplWin32_AdjustWindowRect(viewport, &rect, vd->DwStyle, vd->DwExStyle);
        ::SetWindowPos(vd->Hwnd, insert_after, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, swp_flag | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ::ShowWindow(vd->Hwnd, SW_SHOWNA);
        viewport->PlatformRequestMove = viewport->PlatformRequestResize = true;
    }
}

static ImVec2 ImGui_ImplWin32_GetWindowPos(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    POINT pos = { 0, 0 };
    ::ClientToScreen(vd->Hwnd, &pos);
    return ImVec2((float)pos.x, (float)pos.y);
}

static void ImGui_ImplWin32_UpdateWin32StyleFromWindow(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    vd->DwStyle = ::GetWindowLongW(vd->Hwnd, GWL_STYLE);
    vd->DwExStyle = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE);
}

static void ImGui_ImplWin32_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = { (LONG)pos.x, (LONG)pos.y, (LONG)pos.x, (LONG)pos.y };
    if (viewport->Flags & ImGuiViewportFlags_OwnedByApp)
        ImGui_ImplWin32_UpdateWin32StyleFromWindow(viewport);
    ImGui_ImplWin32_AdjustWindowRect(viewport, &rect, vd->DwStyle, vd->DwExStyle);
    ::SetWindowPos(vd->Hwnd, nullptr, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static ImVec2 ImGui_ImplWin32_GetWindowSize(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect;
    ::GetClientRect(vd->Hwnd, &rect);
    return ImVec2(float(rect.right - rect.left), float(rect.bottom - rect.top));
}

static void ImGui_ImplWin32_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = { 0, 0, (LONG)size.x, (LONG)size.y };
    if (viewport->Flags & ImGuiViewportFlags_OwnedByApp)
        ImGui_ImplWin32_UpdateWin32StyleFromWindow(viewport);
    ImGui_ImplWin32_AdjustWindowRect(viewport, &rect, vd->DwStyle, vd->DwExStyle);
    ::SetWindowPos(vd->Hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

static void ImGui_ImplWin32_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    ::BringWindowToTop(vd->Hwnd);
    ::SetForegroundWindow(vd->Hwnd);
    ::SetFocus(vd->Hwnd);
}

static bool ImGui_ImplWin32_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::GetForegroundWindow() == vd->Hwnd;
}

static bool ImGui_ImplWin32_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::IsIconic(vd->Hwnd) != 0;
}

static void ImGui_ImplWin32_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{

    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    int n = ::MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    ImVector<wchar_t> title_w;
    title_w.resize(n);
    ::MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w.Data, n);

    ::DefWindowProcW(vd->Hwnd, WM_SETTEXT, 0, (LPARAM)title_w.Data);
}

static void ImGui_ImplWin32_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    IM_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
    if (alpha < 1.0f)
    {
        DWORD ex_style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) | WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, ex_style);
        ::SetLayeredWindowAttributes(vd->Hwnd, 0, (BYTE)(255 * alpha), LWA_ALPHA);
    }
    else
    {
        DWORD ex_style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, ex_style);
    }
}

static float ImGui_ImplWin32_GetWindowDpiScale(ImGuiViewport* viewport)
{
    ImGui_ImplWin32_ViewportData* vd = (ImGui_ImplWin32_ViewportData*)viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ImGui_ImplWin32_GetDpiScaleForHwnd(vd->Hwnd);
}

static void ImGui_ImplWin32_OnChangedViewport(ImGuiViewport* viewport)
{
    (void)viewport;
#if 0
    ImGuiStyle default_style;

    default_style.ScaleAllSizes(viewport->DpiScale);
    ImGuiStyle& style = ImGui::GetStyle();
    style = default_style;
#endif
}

namespace ImGui { extern ImGuiIO& GetIO(ImGuiContext*); extern ImGuiPlatformIO& GetPlatformIO(ImGuiContext*); }

static LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler_PlatformWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    ImGuiContext* ctx = (ImGuiContext*)::GetPropA(hWnd, "IMGUI_CONTEXT");
    if (ctx == NULL)
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);

    ImGuiIO& io = ImGui::GetIO(ctx);
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO(ctx);
    LRESULT result = 0;
    if (ImGui_ImplWin32_WndProcHandlerEx(hWnd, msg, wParam, lParam, io))
        result = 1;
    else if (ImGuiViewport* viewport = ImGui_ImplWin32_FindViewportByPlatformHandle(platform_io, hWnd))
    {
        switch (msg)
        {
        case WM_CLOSE:
            viewport->PlatformRequestClose = true;
            return 0;
        case WM_MOVE:
            viewport->PlatformRequestMove = true;
            break;
        case WM_SIZE:
            viewport->PlatformRequestResize = true;
            break;
        case WM_MOUSEACTIVATE:
            if (viewport->Flags & ImGuiViewportFlags_NoFocusOnClick)
                result = MA_NOACTIVATE;
            break;
        case WM_NCHITTEST:

            if (viewport->Flags & ImGuiViewportFlags_NoInputs)
                result = HTTRANSPARENT;
            break;
        }
    }
    if (result == 0)
        result = ::DefWindowProcW(hWnd, msg, wParam, lParam);
    return result;
}

static bool ImGui_ImplWin32_InitMultiViewportSupport(bool platform_has_own_dc)
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    WindowIdentity::Generate(bd->WindowClassName);
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | (platform_has_own_dc ? CS_OWNDC : 0);
    wcex.lpfnWndProc = ImGui_ImplWin32_WndProcHandler_PlatformWindow;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = ::GetModuleHandle(nullptr);
    wcex.hIcon = nullptr;
    wcex.hCursor = nullptr;
    wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = bd->WindowClassName;
    wcex.hIconSm = nullptr;
    const bool registered = ::RegisterClassExW(&wcex) != 0;

    if (!registered) return false;

    ImGui_ImplWin32_UpdateMonitors();

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplWin32_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplWin32_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplWin32_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplWin32_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplWin32_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplWin32_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplWin32_GetWindowSize;
    platform_io.Platform_SetWindowFocus = ImGui_ImplWin32_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplWin32_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplWin32_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplWin32_SetWindowTitle;
    platform_io.Platform_SetWindowAlpha = ImGui_ImplWin32_SetWindowAlpha;
    platform_io.Platform_UpdateWindow = ImGui_ImplWin32_UpdateWindow;
    platform_io.Platform_GetWindowDpiScale = ImGui_ImplWin32_GetWindowDpiScale;
    platform_io.Platform_OnChangedViewport = ImGui_ImplWin32_OnChangedViewport;

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui_ImplWin32_ViewportData* vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    vd->Hwnd = bd->hWnd;
    vd->HwndOwned = false;
    main_viewport->PlatformUserData = vd;
    return true;
}

static void ImGui_ImplWin32_ShutdownMultiViewportSupport()
{
    ImGui_ImplWin32_Data* bd = ImGui_ImplWin32_GetBackendData();
    ImGui::DestroyPlatformWindows();
    const bool removed = ::UnregisterClassW(bd->WindowClassName, ::GetModuleHandle(nullptr)) != FALSE;

}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
