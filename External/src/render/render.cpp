#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "render.h"
#include "../core/app/RuntimePaths.h"
#include "RetroUi.h"
#include "MenuMotion.h"
#include "OverlayLifecycle.h"
#include "WeaponPreview.h"
#include "../core/features/TargetLabels.h"
#include "StartupLoader.h"
#include "../core/net/game_info.h"
#include "SadblobVideo.h"
#include "../core/net/performance.h"
#include "floating_panels.h"
#include "../core/functions/settings/preferences.h"

#include <dwmapi.h>
#include <cmath>
#include <algorithm>
#include <functional>
#include "menu/library.h"
#include "../../ext/imgui/imgui_impl_win32.h"
#include "../../ext/imgui/imgui_impl_dx11.h"
#include "../../src/core/variables/variables.h"
#include "../../src/core/globals/globals.h"
#include "../../src/core/functions/settings/settings.h"
#include "../../src/core/functions/mics/mics.h"
#include "../../src/core/functions/aim/silent_fov_center.h"
#include "../../src/core/functions/aim/aim.h"
#include "../../src/core/functions/skins/skins.h"
#include "../../src/core/keys/keys.h"

#include "../../src/core/features/mesh/shader/MeshDxShader.h"
#include "../../src/core/features/mesh/chams/MeshChams.h"
#include "../../src/core/features/native/NativeChams.h"
#include "../../src/core/features/spotify/music_host_bind.h"
#include "../../src/core/features/spotify/music_player_ui.h"
#include "../../src/core/features/spotify/media.h"
#include "../../src/core/cache/cache.h"
#include "../../src/sdk/offsets.h"
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>

#pragma comment(lib, "windowscodecs.lib")

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_KEYMENU) return 0;
    if (msg == WM_CLOSE) {
        Globals::running = false;
        return 0;
    }
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    if (msg == WM_DESTROY) {
        Globals::running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

OverlayWindow::OverlayWindow() : windowHandle(nullptr), d3dDevice(nullptr), d3dContext(nullptr), swapChain(nullptr), renderTarget(nullptr), overlayWidth(0), overlayHeight(0), composited(false), swapChain1(nullptr), dcompDevice(nullptr), dcompTarget(nullptr), dcompVisual(nullptr) {
    ZeroMemory(&windowClass, sizeof(windowClass));
}

void OverlayWindow::SetupD3D11(HWND hwnd) {

    DXGI_SWAP_CHAIN_DESC1 sd1{};
    sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.SampleDesc.Count = 1;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = 2;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    {
        RECT gr{};
        const HWND game = FindWindowW(nullptr, L"Roblox");
        if (game && GetClientRect(game, &gr) && gr.right > gr.left && gr.bottom > gr.top) {
            sd1.Width = static_cast<UINT>(gr.right - gr.left);
            sd1.Height = static_cast<UINT>(gr.bottom - gr.top);
        } else {
            sd1.Width = static_cast<UINT>(GetSystemMetrics(SM_CXSCREEN));
            sd1.Height = static_cast<UINT>(GetSystemMetrics(SM_CYSCREEN));
        }
    }

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevelArray, 2, D3D11_SDK_VERSION, &d3dDevice, &featureLevel, &d3dContext);
    if (SUCCEEDED(hr)) {
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory2;
        hr = d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        if (SUCCEEDED(hr))
            hr = dxgiDevice->GetAdapter(&adapter);
        if (SUCCEEDED(hr))
            hr = adapter->GetParent(IID_PPV_ARGS(&factory2));
        if (SUCCEEDED(hr))
            hr = factory2->CreateSwapChainForComposition(d3dDevice, &sd1, nullptr, &swapChain1);
        if (SUCCEEDED(hr))
            hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&dcompDevice));
        if (SUCCEEDED(hr))
            hr = dcompDevice->CreateTargetForHwnd(hwnd, TRUE, &dcompTarget);
        if (SUCCEEDED(hr))
            hr = dcompDevice->CreateVisual(&dcompVisual);
        if (SUCCEEDED(hr))
            hr = dcompVisual->SetContent(swapChain1);
        if (SUCCEEDED(hr))
            hr = dcompTarget->SetRoot(dcompVisual);
        if (SUCCEEDED(hr))
            hr = dcompDevice->Commit();
        if (SUCCEEDED(hr)) {
            composited = true;
            overlayWidth = static_cast<int>(sd1.Width);
            overlayHeight = static_cast<int>(sd1.Height);
            swapChain = swapChain1;
            CreateRenderTarget();
            if (FILE* lf = nullptr; _wfopen_s(&lf, RuntimePaths::File(L"goatgoatgoat.log").c_str(), L"a") == 0 && lf) {
                std::fprintf(lf, "[overlay] DirectComposition premultiplied path active (%ux%u)\n", sd1.Width, sd1.Height);
                std::fclose(lf);
            }
            return;
        }
        if (FILE* lf = nullptr; _wfopen_s(&lf, RuntimePaths::File(L"goatgoatgoat.log").c_str(), L"a") == 0 && lf) {
            std::fprintf(lf, "[overlay] DComp init failed hr=0x%08lX - legacy color-key fallback\n", (unsigned long)hr);
            std::fclose(lf);
        }

        if (dcompVisual) { dcompVisual->Release(); dcompVisual = nullptr; }
        if (dcompTarget) { dcompTarget->Release(); dcompTarget = nullptr; }
        if (dcompDevice) { dcompDevice->Release(); dcompDevice = nullptr; }
        if (swapChain1) { swapChain1->Release(); swapChain1 = nullptr; }
        if (d3dContext) { d3dContext->Release(); d3dContext = nullptr; }
        if (d3dDevice) { d3dDevice->Release(); d3dDevice = nullptr; }
    }

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &swapChain, &d3dDevice, &featureLevel, &d3dContext) != S_OK)
        return;
    ComPtr<IDXGIFactory> windowFactory;
    if (SUCCEEDED(swapChain->GetParent(IID_PPV_ARGS(&windowFactory))))
        windowFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    CreateRenderTarget();
}

void OverlayWindow::CleanupD3D11() {
    if (d3dContext) d3dContext->ClearState();
    if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
    if (dcompVisual) { dcompVisual->Release(); dcompVisual = nullptr; }
    if (dcompTarget) { dcompTarget->Release(); dcompTarget = nullptr; }
    if (dcompDevice) { dcompDevice->Release(); dcompDevice = nullptr; }

    if (swapChain1) { swapChain1->Release(); swapChain1 = nullptr; }
    else if (swapChain) swapChain->Release();
    swapChain = nullptr;
    if (d3dContext) { d3dContext->Release(); d3dContext = nullptr; }
    if (d3dDevice) { d3dDevice->Release(); d3dDevice = nullptr; }
    bufferWidth = bufferHeight = 0;
}

void OverlayWindow::CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (swapChain && SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)))) {
        if (SUCCEEDED(d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTarget))) {
            D3D11_TEXTURE2D_DESC desc{}; backBuffer->GetDesc(&desc);
            bufferWidth = desc.Width; bufferHeight = desc.Height;
        }
        backBuffer->Release();
    }
}

struct OverlayMouseEvent { UINT msg; LONG x, y; int wheel; USHORT xdata; bool swallow; };
static std::deque<OverlayMouseEvent> g_mouseEvents;
static std::mutex g_mouseEventsMtx;
static std::thread g_hookThread;
static std::atomic<bool> g_hookStop{ false };
static HANDLE g_mouseEvent = nullptr;
static std::atomic<bool> g_keyboardFocusRequested{false};
static ImVec2 g_menuRectMin(FLT_MAX, FLT_MAX);
static ImVec2 g_menuRectMax(-FLT_MAX, -FLT_MAX);
static ImVec2 g_dlgRectMin(FLT_MAX, FLT_MAX);
static ImVec2 g_dlgRectMax(-FLT_MAX, -FLT_MAX);
static POINT g_overlayOrigin{ 0, 0 };

static std::vector<ImRect> g_uiRects;
static std::mutex g_uiRectsMtx;
static void OverlayTrackUiRect(float x, float y, float w, float h)
{
    std::lock_guard<std::mutex> lk(g_uiRectsMtx);
    if (g_uiRects.size() < 64)
        g_uiRects.push_back(ImRect(ImVec2(x, y), ImVec2(x + w, y + h)));
}
static void OverlayClearUiRects()
{
    std::lock_guard<std::mutex> lk(g_uiRectsMtx);
    g_uiRects.clear();
}

static bool OverlayRectWantsClick(const POINT& pt) {
    const bool overMenu = variables::menuOpen &&
        pt.x >= g_menuRectMin.x && pt.x <= g_menuRectMax.x &&
        pt.y >= g_menuRectMin.y && pt.y <= g_menuRectMax.y;
    const bool overDlg =
        pt.x >= g_dlgRectMin.x && pt.x <= g_dlgRectMax.x &&
        pt.y >= g_dlgRectMin.y && pt.y <= g_dlgRectMax.y;
    if (FloatingPanels::WantsMouse(float(pt.x-g_overlayOrigin.x),float(pt.y-g_overlayOrigin.y))) return true;
    if (overMenu || overDlg)
        return true;

    {
        std::lock_guard<std::mutex> lk(g_uiRectsMtx);
        for (const ImRect& r : g_uiRects) {
            if (pt.x >= r.Min.x && pt.x <= r.Max.x &&
                pt.y >= r.Min.y && pt.y <= r.Max.y)
                return true;
        }
    }
    if (Keys::SpotifyOn() && native_music_player::CursorOverMusicCard())
        return true;
    return false;
}

static LRESULT CALLBACK OverlayMouseHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_mouseEvent) {
        const MSLLHOOKSTRUCT* m = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        bool swallow = false;
        switch (wParam) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {

            swallow = OverlayRectWantsClick(m->pt);
            break;
        }
        default: break;
        }
        static bool capturedLeft=false;
        if(wParam==WM_LBUTTONDOWN){
            capturedLeft=swallow;
            if(swallow&&variables::menuOpen)g_keyboardFocusRequested.store(true,std::memory_order_release);
        }
        if(wParam==WM_LBUTTONUP){swallow=swallow||capturedLeft;capturedLeft=false;}
        OverlayMouseEvent ev{ (UINT)wParam, m->pt.x, m->pt.y, 0, 0, swallow };
        if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL)
            ev.wheel = GET_WHEEL_DELTA_WPARAM(m->mouseData);
        if (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP)
            ev.xdata = HIWORD(m->mouseData);
        {
            std::lock_guard<std::mutex> lk(g_mouseEventsMtx);
            g_mouseEvents.push_back(ev);
            if (g_mouseEvents.size() > 64)
                g_mouseEvents.pop_front();
        }
        SetEvent(g_mouseEvent);
        return swallow ? 1 : CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static void OverlayHookThreadMain() {
    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, OverlayMouseHook, nullptr, 0);
    if (!hook) return;
    while (!g_hookStop.load(std::memory_order_acquire)) {
        DWORD w = MsgWaitForMultipleObjectsEx(1, &g_mouseEvent, 200, QS_ALLINPUT, 0);
        if (w == WAIT_OBJECT_0)
            ResetEvent(g_mouseEvent);
        if (g_hookStop.load(std::memory_order_acquire)) break;

        MSG pumpMsg;
        while (PeekMessageW(&pumpMsg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&pumpMsg);
            DispatchMessageW(&pumpMsg);
        }
    }
    UnhookWindowsHookEx(hook);
}

void DrainOverlayMouseEvents() {
    std::deque<OverlayMouseEvent> local;
    {
        std::lock_guard<std::mutex> lk(g_mouseEventsMtx);
        local.swap(g_mouseEvents);
    }
    if (local.empty()) return;
    if (!ImGui::GetCurrentContext()) return;
    ImGuiIO& io = ImGui::GetIO();
    for (const auto& ev : local) {
        const POINT pt{ ev.x - g_overlayOrigin.x, ev.y - g_overlayOrigin.y };
        switch (ev.msg) {
        case WM_MOUSEMOVE:
            if (variables::menuOpen || Keys::SpotifyOn() || variables::Misc::keybinds || variables::Misc::performancePanel)
                io.AddMousePosEvent((float)pt.x, (float)pt.y);
            break;
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            if (ev.swallow) io.AddMouseButtonEvent(0, ev.msg == WM_LBUTTONDOWN);
            break;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            if (ev.swallow) io.AddMouseButtonEvent(1, ev.msg == WM_RBUTTONDOWN);
            break;
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            if (ev.swallow) io.AddMouseButtonEvent(2, ev.msg == WM_MBUTTONDOWN);
            break;
        case WM_XBUTTONDOWN: case WM_XBUTTONUP:

            if (ev.swallow) io.AddMouseButtonEvent(
                ev.xdata == XBUTTON1 ? 3 : 4, ev.msg == WM_XBUTTONDOWN);
            break;
        case WM_MOUSEWHEEL:
            if (ev.swallow) io.AddMouseWheelEvent(0.0f, ev.wheel / (float)WHEEL_DELTA);
            break;
        case WM_MOUSEHWHEEL:
            if (ev.swallow) io.AddMouseWheelEvent(ev.wheel / (float)WHEEL_DELTA, 0.0f);
            break;
        default: break;
        }
    }
}

void OverlayWindow::SyncToGameWindow() {

    static DWORD s_lastSync = 0;
    const DWORD nowTick = GetTickCount();
    if (s_lastSync != 0 && nowTick - s_lastSync < 100)
        return;
    s_lastSync = nowTick;

    static bool s_sizedToGame = false;
    const HWND game = FindWindowW(nullptr, L"Roblox");
    if (game && IsIconic(game)) return;
    RECT gameRect{};
    int x = 0, y = 0, w = 0, h = 0;
    if (game && GetClientRect(game, &gameRect)) {
        POINT pt{ 0, 0 };
        ClientToScreen(game, &pt);
        w = gameRect.right - gameRect.left;
        h = gameRect.bottom - gameRect.top;
        x = pt.x;
        y = pt.y;
    }
    if (w <= 0 || h <= 0) {
        if (s_sizedToGame)
            return;
        x = 0;
        y = 0;
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
        if (w <= 0 || h <= 0)
            return;
    } else {
        s_sizedToGame = true;
    }
    RECT overlayRect{};
    if (!GetWindowRect(windowHandle, &overlayRect) || overlayRect.left != x || overlayRect.top != y
        || overlayRect.right - overlayRect.left != w || overlayRect.bottom - overlayRect.top != h)
        SetWindowPos(windowHandle, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    g_overlayOrigin.x = x;
    g_overlayOrigin.y = y;
    overlayWidth = w;
    overlayHeight = h;
    ResizeSwapChain(overlayWidth, overlayHeight);
}

void OverlayWindow::ResizeSwapChain(int width, int height) {
    if (!swapChain || width <= 0 || height <= 0)
        return;
    const HRESULT result = OverlayLifecycle::ResizeBackBuffer(d3dDevice, d3dContext,
        swapChain, renderTarget, static_cast<UINT>(width), static_cast<UINT>(height), bufferWidth, bufferHeight);
    if (result == S_FALSE) return;
    if (SUCCEEDED(result)) {
        Cheat::Visuals::MeshDxShader::Resize(bufferWidth, bufferHeight);
    } else {
        static HRESULT lastFailure = S_OK;
        if (result != lastFailure) {
            if (FILE* log = nullptr; _wfopen_s(&log, RuntimePaths::File(L"goatgoatgoat.log").c_str(), L"a") == 0 && log) {
                std::fprintf(log, "[overlay] resize %dx%d failed hr=0x%08lX; target=%d\n", width, height, (unsigned long)result, renderTarget != nullptr);
                std::fclose(log);
            }
            lastFailure = result;
        }
        if (OverlayLifecycle::DeviceLost(result)) Globals::running = false;
    }
}

bool OverlayWindow::Initialize() {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, OverlayWndProc, 0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"ExternalOverlay", nullptr };
    windowClass = wc;
    RegisterClassExW(&windowClass);
    windowHandle = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        windowClass.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!windowHandle || !CaptureProtection::Apply(windowHandle, StartupLoader::StreamproofEnabled()))
        return false;

    ShowWindow(windowHandle, SW_SHOW);

    UpdateWindow(windowHandle);
    SetupD3D11(windowHandle);
    if (!d3dDevice || !d3dContext || !swapChain || !renderTarget) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImFontConfig pixel;
    pixel.SizePixels=13; pixel.OversampleH=1; pixel.OversampleV=1; pixel.PixelSnapH=true;
    ImFont* menuFont=UiAssets::Font(16);
    io.FontDefault=menuFont;
    auto& fonts=imGuiCustom::GetFontsMutable();
    fonts.InterMedium=fonts.InterSmall=fonts.InterSemiBold=fonts.InterBold=fonts.InterSemiBoldSmall=menuFont;
    imGuiCustom::Initialize(menuFont);
    ImGui_ImplWin32_Init(windowHandle);
    ImGui_ImplDX11_Init(d3dDevice,d3dContext);
    UiText::Load(); Preferences::Load();
    LaunchPrivacy::ApplyToSettings();
    CaptureProtection::Apply(windowHandle, variables::Misc::streamProof);
    Cheat::Visuals::MeshDxShader::Init(d3dDevice, d3dContext);
    Cheat::Visuals::MeshDxShader::Resize(static_cast<unsigned>(overlayWidth), static_cast<unsigned>(overlayHeight));
    MusicHost::Bind(windowHandle, d3dDevice,
        imGuiCustom::GetFontsMutable().InterMedium,
        imGuiCustom::GetFontsMutable().InterSemiBold);

    g_mouseEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_hookThread = std::thread(OverlayHookThreadMain);
    return true;
}

ImVec2 OverlayWindow::GetClientSize() const {
    return ImVec2(static_cast<float>(overlayWidth), static_cast<float>(overlayHeight));
}

void OverlayWindow::UpdateWindowStyle(bool inputWanted) {
    const bool clickedMenu=g_keyboardFocusRequested.exchange(false,std::memory_order_acq_rel);
    if (!composited) {

        const LONG baseEx = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
        const LONG wantStyle = inputWanted ? baseEx : (baseEx | WS_EX_TRANSPARENT);
        if (wantStyle != lastExStyle) {
            SetWindowLong(windowHandle, GWL_EXSTYLE, wantStyle);
            lastExStyle = wantStyle;
        }
        if(!clickedMenu)return;
    }

    static int s_lastFocus = -1;
    if ((int)inputWanted == s_lastFocus && !clickedMenu)
        return;
    s_lastFocus = inputWanted;
    if (inputWanted) {
        const HWND fg = GetForegroundWindow();
        DWORD fgThread = 0;
        if (fg)
            fgThread = GetWindowThreadProcessId(fg, nullptr);
        const DWORD me = GetCurrentThreadId();
        const bool attached = fg && fgThread && fgThread != me;
        if (attached)
            AttachThreadInput(fgThread, me, TRUE);
        SetActiveWindow(windowHandle);
        SetForegroundWindow(windowHandle);
        SetFocus(windowHandle);
        if (attached)
            AttachThreadInput(fgThread, me, FALSE);
    } else {
        if (GetFocus() == windowHandle)
            SetFocus(nullptr);
    }
}

void OverlayWindow::BeginFrame() {
    SyncToGameWindow();
    imGuiCustom::g_fontScale = 1.25f;
    imGuiCustom::Theme& theme = imGuiCustom::GetThemeMutable();
    theme.WindowBg = variables::Theme::background;
    theme.CardBg = variables::Theme::panels;
    theme.ControlBg = variables::Theme::controls;
    theme.ControlInactive = imGuiCustom::LerpColor(variables::Theme::controls, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.12f);
    theme.Accent = variables::Theme::accent;
    theme.AccentText = variables::Theme::accent;
    theme.Text = variables::Theme::text;
    theme.TextBright = variables::Theme::textBright;
    const bool spotifyInput = Keys::SpotifyOn() && native_music_player::CursorOverMusicCard();
    POINT mouse{};GetCursorPos(&mouse);
    const bool panelsInput=FloatingPanels::WantsMouse(float(mouse.x-g_overlayOrigin.x),float(mouse.y-g_overlayOrigin.y));
    UpdateWindowStyle(variables::menuOpen || spotifyInput || (!composited && panelsInput));
    static DWORD s_lastAffinity = 0xFFFFFFFFul;
    const DWORD wantAffinity = Keys::StreamProofOn() ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    if (wantAffinity != s_lastAffinity) {
        if (CaptureProtection::Apply(windowHandle, wantAffinity != WDA_NONE))
            s_lastAffinity = wantAffinity;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

namespace {

constexpr float kMenuW = 940.0f;
constexpr float kMenuH = 630.0f;
constexpr float kSidebarW = 164.0f;
constexpr float kContentTop = 56.0f;
constexpr float kContentBottom = 517.0f;

constexpr float kFrameW = kMenuW;
constexpr float kChildX = kSidebarW + 22.0f;
constexpr float kChildY = kContentTop + 13.0f;
constexpr float kChildW = kFrameW - kChildX - 20.0f;
constexpr float kChildH = kContentBottom - kChildY - 16.0f;

inline ImU32 PrimText()    { const imGuiCustom::Theme& t = imGuiCustom::GetTheme(); return ImGui::GetColorU32(t.Text); }

struct PrimTabDef { const char* label; const char* icon; };
constexpr PrimTabDef kTabs[]={{"AIM",""},{"VISUALS",""},{"SYSTEM",""},{"SKINS",""}};
constexpr int kTabCount=4;
struct PrimSubTabDef {const char* label;const char* desc;};
constexpr PrimSubTabDef kSubTabs[kTabCount][6]={
    {{"Silent Aim",""},{"Checks",""},{"Recoil",""},{"",""}},
    {{"Mesh",""},{"Native",""},{"Target checks",""}},
    {{"General",""},{"Players",""},{"Appearance",""},{"",""}},
    {{"Skin Changer",""},{"Custom skin\noverlay",""},{"Skin editor",""}}
};
int SubTabCount(int tab){int n=0;for(int i=0;i<6;++i)if(kSubTabs[tab][i].label && kSubTabs[tab][i].label[0])++n;return n;}

void PrimContent(const char* id, const char* title, const ImVec2& pos, const ImVec2& size,
                 const std::function<void()>& body) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 win = ImGui::GetWindowPos();
    const ImVec2 min = ImVec2(std::floor(win.x + pos.x + imGuiCustom::g_contentOffset.x),
                              std::floor(win.y + pos.y + imGuiCustom::g_contentOffset.y));
    const ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
    RetroUi::Frame(dl,min,max);
    RetroUi::Stars(dl,min,size,float(ImGui::GetTime()));
    if (SadblobVideo::Texture()) {

        dl->PushClipRect(min, max, true);
        const float clipAspect = SadblobVideo::AspectRatio();
        const float panelAspect = size.x / size.y;
        ImVec2 uv0(0.0f, 0.0f), uv1(1.0f, 1.0f);
        if (clipAspect > panelAspect) {
            const float side = (1.0f - panelAspect / clipAspect) * 0.5f;
            uv0.x = side; uv1.x = 1.0f - side;
        } else {
            const float top = (1.0f - clipAspect / panelAspect) * 0.5f;
            uv0.y = top; uv1.y = 1.0f - top;
        }
        dl->AddImage((ImTextureID)(intptr_t)(uintptr_t)SadblobVideo::Texture(), min, max,
            uv0, uv1, IM_COL32(255, 255, 255, 220));
        auto tint=variables::Theme::background;tint.w=.48f;
        dl->AddRectFilled(min, max, ImGui::GetColorU32(tint));
        dl->PopClipRect();
    }

    dl->AddRect(min,max,ImGui::GetColorU32(variables::Theme::accent),0,0,2);
        const imGuiCustom::Fonts& fonts = imGuiCustom::GetFonts();
    if (fonts.InterBold)
        dl->AddText(fonts.InterBold, 16.0f, ImVec2(min.x + 14.0f, min.y + 14.0f), PrimText(), UiText::Tr(title));
    ImGui::PushID(id);

    ImGui::SetCursorScreenPos(min + ImVec2(6.0f, 0.0f));
    ImGui::BeginChild(id, ImVec2(size.x - 12.0f, size.y - 4.0f), ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove);
    body();
    ImGui::EndChild();
    ImGui::PopID();
}

}

void OverlayWindow::RenderMenu() {
    if(!variables::menuOpen && !MenuMotion::Visible()){
        SadblobVideo::Update(d3dDevice,d3dContext,false,variables::Theme::sadblobStyle);
        OverlayClearUiRects();
        return;
    }
    if(variables::Theme::preset==SadblobVideo::ThemePreset && !SadblobVideo::HasBundledClips())
        variables::Theme::preset=SadblobVideo::CustomPreset;
    SadblobVideo::SetCustomPath(variables::Theme::preset==SadblobVideo::CustomPreset?std::filesystem::u8path(variables::Theme::customMp4Path).wstring():UiAssets::Path(201,L"grid.mp4"));
    SadblobVideo::Update(d3dDevice,d3dContext,true,
        variables::Theme::preset==SadblobVideo::ThemePreset?variables::Theme::sadblobStyle:SadblobVideo::StyleCount);
    static int s_subTab=0,previousTab=-1;
    variables::selectedTab=std::clamp(variables::selectedTab,0,kTabCount-1);
    if(previousTab!=variables::selectedTab){s_subTab=0;previousTab=variables::selectedTab;}
    ImGui::SetNextWindowSize(ImVec2(kMenuW,kMenuH),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(60,50),ImGuiCond_FirstUseEver);
    ImGui::Begin("goatgoatgoatgoatgoatgoat",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|(!variables::menuOpen?ImGuiWindowFlags_NoInputs:0));
    auto draw=ImGui::GetWindowDrawList();auto w=ImGui::GetWindowPos();
    RetroUi::Frame(draw,w,w+ImVec2(kMenuW,kMenuH));
    ImGui::SetCursorScreenPos(w+ImVec2(8,6));
    ImGui::InvisibleButton("##drag_retro",ImVec2(kMenuW-84,34));
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0))ImGui::SetWindowPos(w+ImGui::GetIO().MouseDelta);
    if(RetroUi::CaptionButton("##minimize",w+ImVec2(kMenuW-66,8),false)){
        variables::menuOpen=false; imGuiCustom::KeyCaptureId()=0; ImGui::ClearActiveID();
    }
    if(RetroUi::CaptionButton("##close",w+ImVec2(kMenuW-36,8),true))ImGui::OpenPopup("Close program?");
    RetroUi::Stars(draw,w+ImVec2(8,6),ImVec2(kMenuW-100,35),float(ImGui::GetTime()));
    draw->AddText(ImGui::GetFont(),26,w+ImVec2(24,12),ImGui::GetColorU32(variables::Theme::textBright),"goatgoatgoatgoatgoatgoat");
    for(int i=0;i<kTabCount;++i){
        ImGui::SetCursorPos(ImVec2(16+i*230.f,51));
        if(RetroUi::Choice(kTabs[i].label,variables::selectedTab==i,ImVec2(218,42),i)){variables::selectedTab=i;s_subTab=0;imGuiCustom::KeyCaptureId()=0;}
    }
    RetroUi::Frame(draw,w+ImVec2(16,111),w+ImVec2(180,607));
    for(int i=0;i<SubTabCount(variables::selectedTab);++i){
        ImGui::SetCursorPos(ImVec2(26,130+i*48.f));
        if(RetroUi::Choice(kSubTabs[variables::selectedTab][i].label,s_subTab==i,ImVec2(144,35))){s_subTab=i;imGuiCustom::KeyCaptureId()=0;}
    }
    draw->PushClipRect(w+ImVec2(25,518),w+ImVec2(173,597),true);
    draw->AddText(w+ImVec2(28,532),ImGui::GetColorU32(variables::Theme::text),GameInfo::Name().c_str());
    draw->AddText(ImGui::GetFont(),12,w+ImVec2(28,570),ImGui::GetColorU32(variables::Theme::text),Offsets::ClientVersion.c_str());
    draw->PopClipRect();
    const ImVec2 childPos(196,111),childSize(728,496);
    const int tab=variables::selectedTab,sub=s_subTab;
    imGuiCustom::g_contentOffset=ImVec2(0,0);
    switch (tab) {
    case 0: {

        if (sub == 0)
            PrimContent("aim_main", "Silent Aim", childPos, childSize,
                [] { Settings::RenderAimMain(); });
        else if(sub==1)
            PrimContent("aim_checks", "Checks", childPos, childSize,
                [] { Settings::RenderAimChecks(); });
        else
            PrimContent("aim_recoil", "Recoil dampening", childPos, childSize,
                [] { Settings::RenderRecoil(); });
        break;
    }
    case 1: {

        const char* subName = sub==0?"Chams":sub==1?"Native":"Target checks";
        PrimContent("visual_main", subName, childPos, childSize,
            [sub] {
                const ImVec2 base = ImGui::GetWindowPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if(sub==0 || sub==1){
                    bool* unlimited=sub==0?&variables::ESP::meshUnlimited:&variables::ESP::nativeUnlimited;
                    float* distance=sub==0?&variables::ESP::meshDistance:&variables::ESP::nativeDistance;
                    imGuiCustom::Checkbox("Unlimited distance",unlimited,ImVec2(317,51));
                    if(!*unlimited)imGuiCustom::SliderFloat("draw_distance",distance,1,1000000,ImVec2(317,82+imGuiCustom::SliderTop()),360,"Draw Distance (studs)","%.0f");
                    dl->AddText(base+ImVec2(317,155),PrimText(),"Applies to loaded characters.");
                    if(sub==1){
                        dl->AddText(base+ImVec2(317,184),PrimText(),Cheat::Visuals::NativeChams::StartupStatus());
                        const auto note=Cheat::Visuals::NativeChams::OcclusionNote();
                        if(note && *note)dl->AddText(ImGui::GetFont(),13,base+ImVec2(317,215),PrimText(),note,nullptr,360);
                    }
                }
                if (sub == 0) {
                    float ly = 51.0f;
                    imGuiCustom::Checkbox("Mesh Chams", &variables::ESP::meshChams, ImVec2(12.0f, ly));
                    imGuiCustom::ColorSquare("mesh_fill", reinterpret_cast<ImVec4*>(variables::ESP::chamsFillColor), ImVec2(264.0f, ly + 1.0f));
                    ly += imGuiCustom::CheckStep();
                    imGuiCustom::SliderFloat("mesh_opacity", &variables::ESP::meshChamsOpacity, 0.0f, 1.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Chams Opacity", "%.2f");
                    ly += imGuiCustom::SliderStep();
                    const char* const* modes = Cheat::Visuals::MeshDxShader::ModeNames();
                    imGuiCustom::Combo("mesh_mode", &variables::ESP::meshChamsDxMode, modes, Cheat::Visuals::MeshDxShader::ModeNameCount(), ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 220.0f, "Visible Style:");
                    ly += imGuiCustom::ComboStep() + imGuiCustom::ComboTop();
                    imGuiCustom::ColorSquare("mesh_occluded", reinterpret_cast<ImVec4*>(variables::ESP::meshChamsOccludedColor), ImVec2(264.0f, ly + 1.0f));
                    imGuiCustom::Checkbox("Occlusion", &variables::ESP::meshChamsOcclusion, ImVec2(12.0f, ly));
                    ly += imGuiCustom::CheckStep();

                    imGuiCustom::Checkbox("Union walls", &variables::ESP::meshChamsUnionWalls, ImVec2(12.0f, ly));
                    ly += imGuiCustom::CheckStep();
                    imGuiCustom::Combo("mesh_occ_mode", &variables::ESP::meshChamsOccludedDxMode, modes, Cheat::Visuals::MeshDxShader::ModeNameCount(), ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 220.0f, "Occluded Style:");
                    ly += imGuiCustom::ComboStep() + imGuiCustom::ComboTop();
                    imGuiCustom::Checkbox("Outline", &variables::ESP::meshChamsOutline, ImVec2(12.0f, ly));
                    imGuiCustom::ColorSquare("mesh_outline", reinterpret_cast<ImVec4*>(variables::ESP::meshChamsOutlineColor), ImVec2(264.0f, ly + 1.0f));
                    ly += imGuiCustom::CheckStep();
                    const char* const* outlines = Cheat::Visuals::MeshChams::OutlineStyleNames();
                    imGuiCustom::Combo("mesh_outline_style", &variables::ESP::meshChamsOutlineStyle, outlines, Cheat::Visuals::MeshChams::OutlineStyleNameCount(), ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 158.0f, "Outline Style:");
                    ly += imGuiCustom::ComboStep() + imGuiCustom::ComboTop();
                    imGuiCustom::SliderFloat("mesh_outline_fade", &variables::ESP::meshChamsOutlineFade, 0.0f, 1.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Outline Fade", "%.2f");
                } else if (sub == 1) {
                    imGuiCustom::Checkbox("Show native preview",&variables::ESP::nativePreview,ImVec2(317,231));

                    float ly = 51.0f;
                    imGuiCustom::Checkbox("Native Chams", &variables::ESP::nativeChams, ImVec2(12.0f, ly));
                    imGuiCustom::ColorSquare("native_fill", reinterpret_cast<ImVec4*>(variables::ESP::nativeChamsColor), ImVec2(264.0f, ly + 1.0f));
                    ly += imGuiCustom::CheckStep();
                    imGuiCustom::SliderFloat("native_opacity", &variables::ESP::nativeChamsOpacity, 0.0f, 1.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Chams Opacity", "%.2f");
                    ly += imGuiCustom::SliderStep();
                    imGuiCustom::Checkbox("Chams only (hide avatars)", &variables::ESP::nativeChamsOnly, ImVec2(12.0f, ly));
                    ly += imGuiCustom::CheckStep();
                    const char* const* styles = Cheat::Visuals::NativeChams::ShaderNames();
                    imGuiCustom::Combo("native_style", &variables::ESP::nativeChamsStyle, styles, Cheat::Visuals::NativeChams::ShaderNameCount(), ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 220.0f, "Style:");
                    ly += imGuiCustom::ComboStep() + imGuiCustom::ComboTop();
                    imGuiCustom::Checkbox("Occlusion", &variables::ESP::nativeChamsOcclusion, ImVec2(12.0f, ly));
                    imGuiCustom::ColorSquare("native_occluded", reinterpret_cast<ImVec4*>(variables::ESP::nativeChamsOccludedColor), ImVec2(264.0f, ly + 1.0f));
                    ly += imGuiCustom::CheckStep();
                    if (variables::ESP::nativeChamsOcclusion) {
                        imGuiCustom::Combo("native_occluded_style", &variables::ESP::nativeChamsOccludedStyle, styles, Cheat::Visuals::NativeChams::ShaderNameCount(), ImVec2(12.0f, ly + imGuiCustom::ComboTop()), 220.0f, "Occluded Style:");
                        ly += imGuiCustom::ComboStep() + imGuiCustom::ComboTop();
                    }
                    imGuiCustom::SliderFloat("native_animation_speed", &variables::ESP::nativeChamsAnimationSpeed, 0.0f, 15.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Animation Speed", "%.2fx");
                    ly += imGuiCustom::SliderStep();
                    imGuiCustom::SliderFloat("native_pattern_size", &variables::ESP::nativeChamsPatternSize, 0.25f, 4.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Style Size", "%.2fx");
                    ly += imGuiCustom::SliderStep();
                    imGuiCustom::Checkbox("Glow", &variables::ESP::nativeChamsGlow, ImVec2(12.0f, ly));
                    imGuiCustom::ColorSquare("native_glow", reinterpret_cast<ImVec4*>(variables::ESP::nativeChamsGlowColor), ImVec2(264.0f, ly + 1.0f));
                    ly += imGuiCustom::CheckStep();
                    imGuiCustom::SliderFloat("native_glow_strength", &variables::ESP::nativeChamsGlowStrength, 0.0f, 2.0f, ImVec2(12.0f, ly + imGuiCustom::SliderTop()), 272.0f, "Glow Strength", "%.2f");
                    ly += imGuiCustom::SliderStep();

                    imGuiCustom::Checkbox("Through Walls", &variables::ESP::nativeChamsWalls, ImVec2(12.0f, ly));
                    ly += imGuiCustom::CheckStep();
                } else {
                    TargetLabels::Menu();

                    float ky=340.0f;
                    imGuiCustom::Checkbox("Visual Keybind", &variables::ESP::visualKeybindEnabled, ImVec2(12.0f,ky));
                    if(variables::ESP::visualKeybindEnabled){
                        ky+=imGuiCustom::CheckStep()+imGuiCustom::ComboTop();
                        const char* modes[]={"Toggle key", "Hold key"};
                        imGuiCustom::Combo("visual_key_mode",&variables::ESP::visualKeybindMode,modes,2,ImVec2(12.0f,ky),190.0f,"Key Mode:");
                        ky+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop()+8.0f;
                        const char* targets[]={"Native", "Mesh Chams"};
                        imGuiCustom::Combo("visual_key_target",&variables::ESP::visualKeybindTarget,targets,2,ImVec2(12.0f,ky),190.0f,"Visual:");
                        ky+=imGuiCustom::ComboStep()+12.0f;
                        dl->AddText(base+ImVec2(12.0f,ky),PrimText(),UiText::Tr("Key:"));
                        imGuiCustom::Keybind("visual_key",&variables::ESP::visualKeybindKey,ImVec2(117.0f,ky),ImVec2(85.0f,15.0f),nullptr,false);
                    }
                    float ry = 51.0f;
                    imGuiCustom::Checkbox("Team Check", &variables::ESP::meshTeamCheck, ImVec2(12.0f, ry));
                    ry += imGuiCustom::CheckStep();
                    imGuiCustom::Checkbox("Dead Check", &variables::ESP::meshDeadCheck, ImVec2(12.0f, ry));
                    ry += imGuiCustom::CheckStep();
                    imGuiCustom::Checkbox("Invisible Check", &variables::ESP::meshInvisibleCheck, ImVec2(12.0f, ry));
                    ry += imGuiCustom::CheckStep();
                    imGuiCustom::Checkbox("Transparency Check", &variables::ESP::meshTransparencyCheck, ImVec2(12.0f, ry));
                    ry += imGuiCustom::CheckStep();
                    imGuiCustom::Checkbox("Local Player", &variables::ESP::meshChamsLocal, ImVec2(12.0f, ry));
                    ry += imGuiCustom::CheckStep();
                    imGuiCustom::SliderFloat("mesh_trans_min", &variables::ESP::meshTransparencyMin, 0.0f, 1.0f, ImVec2(12.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Transparency Min", "%.2f");
                    ry += imGuiCustom::SliderStep();
                    imGuiCustom::SliderFloat("mesh_trans_max", &variables::ESP::meshTransparencyMax, 0.0f, 1.0f, ImVec2(12.0f, ry + imGuiCustom::SliderTop()), 272.0f, "Transparency Max", "%.2f");
                }
            });
        break;
    }
    case 2:
        if(sub==1) PrimContent("misc_players","Players",childPos,childSize,
            []{Mics::RenderPlayersMenu();});
        else if(sub==2) PrimContent("misc_theme","Theme",childPos,childSize,
            []{Mics::RenderThemeMenu();});
        else PrimContent("misc_main", "General", childPos, childSize,
            [] { Mics::RenderMiscMenu(); });
        break;
    case 3:
        if(sub==1)PrimContent("skin_overlay", "Custom skin overlay",childPos,childSize,
            []{const auto base=ImGui::GetWindowPos();auto dl=ImGui::GetWindowDrawList();

                    using namespace variables::Weapons;
                    static int group=0;const char* groups[]={"Weapon","Arms","Gloves"};
                    imGuiCustom::Combo("weapon_group",&group,groups,3,ImVec2(12,51),272,"Customize:");
                    auto& limb=group==2?gloves:arms;
                    float* selectedColor=group?limb.color:color;
                    float* selectedGlow=group?limb.glowColor:glowColor;
                    float y=116;
                    auto check=[&](const char* label,bool* v){imGuiCustom::Checkbox(label,v,ImVec2(12,y));y+=imGuiCustom::CheckStep();};
                    auto slider=[&](const char* id,float* v,float lo,float hi,const char* label){imGuiCustom::SliderFloat(id,v,lo,hi,ImVec2(12,y+imGuiCustom::SliderTop()),272,label,"%.2f");y+=imGuiCustom::SliderStep();};
                    imGuiCustom::ColorSquare("weapon_color",reinterpret_cast<ImVec4*>(selectedColor),ImVec2(264,116));
                    slider("weapon_opacity",group?&limb.opacity:&opacity,0,1,"Opacity");
                    imGuiCustom::Combo("weapon_native_style",group?&limb.style:&nativeStyle,Cheat::Visuals::NativeChams::ShaderNames(),Cheat::Visuals::NativeChams::ShaderNameCount(),ImVec2(12,y+imGuiCustom::ComboTop()),272,"Style:");
                    y+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop();
                    slider("weapon_speed",group?&limb.speed:&speed,0,15,"Animation Speed");slider("weapon_scale",group?&limb.scale:&scale,group?.1f:.01f,group?8.f:100.f,"Pattern Size");
                    check("Glow",group?&limb.glow:&glow);
                    imGuiCustom::ColorSquare("weapon_glow",reinterpret_cast<ImVec4*>(selectedGlow),ImVec2(264,y-imGuiCustom::CheckStep()+1));
                    slider("weapon_glow_strength",group?&limb.glowStrength:&glowStrength,0,2,"Glow Strength");
                    y=51;
                    auto right=[&](const char* label,bool* v){imGuiCustom::Checkbox(label,v,ImVec2(317,y));y+=imGuiCustom::CheckStep();};
                    right("Weapon chams",&native);right("Arm chams",&arms.enabled);right("Glove chams",&gloves.enabled);
                    right("Show original gun",&showOriginal);
                    y+=12;right("Hide weapon and arms",&hide);right("First-person weapon",&viewmodel);right("AR-15",&ar15);right("Glock",&glock);right("Show weapon preview",&preview);
            });
        else if(sub==2)PrimContent("skin_editor","Skin editor",childPos,childSize,
            []{Skins::RenderEditor();});
        else PrimContent("skins_main", "Skin Changer", childPos, childSize,
            [this] { Skins::RenderMenu();WeaponPreview::DrawSkins(d3dDevice,d3dContext,Skins::SavedSelection()); });
        break;
    }

    imGuiCustom::Checkbox("Remember this page",&Preferences::saveTabs[tab],ImVec2(735,610));
    ImGui::SetNextWindowSize(ImVec2(380,176));
    ImGui::SetNextWindowPos(w+ImVec2(280,270),ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Close program?",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove)){
        auto p=ImGui::GetWindowPos();RetroUi::Frame(ImGui::GetWindowDrawList(),p,p+ImVec2(380,176));
        g_dlgRectMin=p;g_dlgRectMax=p+ImVec2(380,176);
        ImGui::SetCursorPos(ImVec2(24,26));ImGui::TextUnformatted("* Close and restore the game?");
        ImGui::SetCursorPos(ImVec2(24,100));if(RetroUi::Choice("YES",false,ImVec2(150,40))){Globals::running=false;ImGui::CloseCurrentPopup();}
        ImGui::SetCursorPos(ImVec2(204,100));if(RetroUi::Choice("NO",false,ImVec2(150,40)))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }else{g_dlgRectMin=ImVec2(FLT_MAX,FLT_MAX);g_dlgRectMax=ImVec2(-FLT_MAX,-FLT_MAX);}
    g_menuRectMin=w;g_menuRectMax=w+ImVec2(kMenuW,kMenuH);
    ImGui::End();

    OverlayClearUiRects();
    if(tab==3&&sub==1&&variables::Weapons::preview){
        WeaponPreview::Draw(d3dDevice,d3dContext,w,ImVec2(kMenuW,kMenuH));
        if(auto* preview=ImGui::FindWindowByName("Weapon preview"))
            OverlayTrackUiRect(preview->Pos.x,preview->Pos.y,preview->Size.x,preview->Size.y);
    }
    if(tab==1&&sub==1&&variables::ESP::nativePreview){
        WeaponPreview::DrawAvatar(d3dDevice,d3dContext,w,ImVec2(kMenuW,kMenuH));
        if(auto* preview=ImGui::FindWindowByName("Native preview"))OverlayTrackUiRect(preview->Pos.x,preview->Pos.y,preview->Size.x,preview->Size.y);
    }
    ImGuiContext& g = *GImGui;
    for (int i = 0; i < g.WindowsFocusOrder.Size; ++i) {
        ImGuiWindow* w = g.WindowsFocusOrder[i];
        if (!w || !(w->Active) || w->Hidden)
            continue;
        if (!(w->Flags & ImGuiWindowFlags_Popup))
            continue;
        const ImRect& r = w->OuterRectClipped;
        if (r.Min.x >= r.Max.x || r.Min.y >= r.Max.y)
            continue;
        OverlayTrackUiRect(r.Min.x, r.Min.y,
            r.Max.x - r.Min.x, r.Max.y - r.Min.y);
    }
    if (GImGui->NavWindow) {
        ImGuiWindow* w = GImGui->NavWindow;
        if ((w->Flags & ImGuiWindowFlags_Popup) && w->Active && !w->Hidden) {
            const ImRect& r = w->OuterRectClipped;
            if (r.Min.x < r.Max.x && r.Min.y < r.Max.y)
                OverlayTrackUiRect(r.Min.x, r.Min.y,
                    r.Max.x - r.Min.x, r.Max.y - r.Min.y);
        }
    }

    for (const ImVec4& r : imGuiCustom::HandDrawnPopupRects()) {
        if (r.z > r.x && r.w > r.y)
            OverlayTrackUiRect(r.x, r.y, r.z - r.x, r.w - r.y);
    }
    for(auto* window:GImGui->Windows){
        if(!window->Active)continue;
        auto* root=window->RootWindow;
        if(std::string(root->Name)!="goatgoatgoatgoatgoatgoat" && std::string(root->Name)!="Weapon preview" && std::string(root->Name)!="Native preview" && !(window->Flags&ImGuiWindowFlags_Popup))continue;
        for(auto& v:window->DrawList->VtxBuffer){unsigned a=(v.col>>24);v.col=(v.col&0xFFFFFF)|(unsigned(a*MenuMotion::alpha)<<24);}
    }
    imGuiCustom::ClearHandDrawnPopupRects();
}

void OverlayWindow::render(ImDrawList* drawList) {

    if (variables::Aimbot::silentEnabled && variables::Aimbot::silentShowFOV) {
        const RBX::Vec2 c = Aimbot::SilentAimCenter();
        const ImVec2 center(c.X, c.Y);
        const float radius = Aimbot::SilentAimRadius();
        const ImU32 fc = imGuiCustom::ColorU32(variables::Aimbot::silentFovColor);

        drawList->AddCircle(center, radius, IM_COL32(8, 8, 8, 255), 64, 2.0f);
        drawList->AddCircle(center, radius, fc, 64, 1.0f);
    }
    FloatingPanels::Draw();
    TargetLabels::Draw(drawList);

}

bool OverlayWindow::EndFrame() {
    if (Keys::SpotifyOn())
        native_music_player::DrawMusicPlayer();
    ImGui::Render();
    if (!renderTarget || !d3dContext || !swapChain) return false;
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dContext->OMSetRenderTargets(1, &renderTarget, nullptr);
    d3dContext->ClearRenderTargetView(renderTarget, clearColor);
    Cheat::Visuals::MeshDxShader::Flush(renderTarget);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    LARGE_INTEGER p0, p1, freq;
    QueryPerformanceCounter(&p0);
    HRESULT presentResult;
    {Performance::Scope timing(Performance::Present);presentResult = OverlayLifecycle::Present(swapChain, false);}
    if (presentResult == DXGI_ERROR_WAS_STILL_DRAWING || presentResult == DXGI_STATUS_OCCLUDED) return false;
    if (FAILED(presentResult)) {
        static HRESULT lastFailure = S_OK;
        if (presentResult != lastFailure) {
            if (FILE* log = nullptr; _wfopen_s(&log, RuntimePaths::File(L"goatgoatgoat.log").c_str(), L"a") == 0 && log) {
                std::fprintf(log, "[overlay] Present failed hr=0x%08lX\n", (unsigned long)presentResult);
                std::fclose(log);
            }
            lastFailure = presentResult;
        }
        if (OverlayLifecycle::DeviceLost(presentResult)) Globals::running = false;
        return false;
    }
    StartupLoader::Dismiss();
    Performance::Frame();
    if (composited && dcompDevice)
        dcompDevice->Commit();
    QueryPerformanceCounter(&p1);
    QueryPerformanceFrequency(&freq);
    {
        static double accPresentMs = 0.0;
        static double accFrameMs = 0.0;
        static int frames = 0;
        static LARGE_INTEGER lastDump = {};
        const double presentMs = (p1.QuadPart - p0.QuadPart) * 1000.0 / freq.QuadPart;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        static LARGE_INTEGER frameStart = {};
        if (frameStart.QuadPart)
            accFrameMs += (now.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart;
        frameStart = now;
        accPresentMs += presentMs;
        ++frames;
        if (!lastDump.QuadPart)
            lastDump = now;
        const double elapsedS = (now.QuadPart - lastDump.QuadPart) / (double)freq.QuadPart;
        if (elapsedS >= 5.0 && frames > 0) {
            FILE* f = nullptr;
            if (_wfopen_s(&f, RuntimePaths::File(L"goatgoatgoat_perf.log").c_str(), L"a") == 0 && f) {
                std::fprintf(f, "[perf] %.1fs: %d presents (%.1f/s), avg present %.2f ms, avg frame-total %.2f ms, menu=%d spotify=%d chams=%d\n",
                    elapsedS, frames, frames / elapsedS, accPresentMs / frames, accFrameMs / frames,
                    variables::menuOpen ? 1 : 0, Keys::SpotifyOn() ? 1 : 0, variables::ESP::meshChams ? 1 : 0);
                std::fclose(f);
            }
            accPresentMs = accFrameMs = 0.0;
            frames = 0;
            lastDump = now;
        }
    }
    return true;
}

void OverlayWindow::Cleanup() {
    WeaponPreview::Shutdown();
    SadblobVideo::Shutdown();
    if (g_hookThread.joinable()) {
        g_hookStop.store(true, std::memory_order_release);
        if (g_mouseEvent) SetEvent(g_mouseEvent);
        g_hookThread.join();
    }
    if (g_mouseEvent) { CloseHandle(g_mouseEvent); g_mouseEvent = nullptr; }
    native_music_player::ShutdownMusicPlayer();
    media::Shutdown();
    Cheat::Visuals::MeshDxShader::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D11();
    if (windowHandle) {
        DestroyWindow(windowHandle);
        windowHandle = nullptr;
    }
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
}
