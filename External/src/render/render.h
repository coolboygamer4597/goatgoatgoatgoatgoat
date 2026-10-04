#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include "../../ext/imgui/imgui.h"

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class OverlayWindow {
private:
    HWND windowHandle;
    WNDCLASSEXW windowClass;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTarget;
    int overlayWidth;
    int overlayHeight;
    UINT bufferWidth = 0;
    UINT bufferHeight = 0;

    bool composited;
    IDXGISwapChain1* swapChain1;
    IDCompositionDevice* dcompDevice;
    IDCompositionTarget* dcompTarget;
    IDCompositionVisual* dcompVisual;

    void SetupD3D11(HWND hwnd);
    void CleanupD3D11();
    void CreateRenderTarget();
    void SyncToGameWindow();
    void ResizeSwapChain(int width, int height);
    LONG lastExStyle = -1;

public:
    OverlayWindow();
    bool Initialize();
    void BeginFrame();
    void UpdateWindowStyle(bool inputWanted);
    void RenderMenu();
    void render(ImDrawList* drawList);
    bool EndFrame();
    void Cleanup();
    HWND GetWindowHandle() const;
    ImVec2 GetClientSize() const;
};

void DrainOverlayMouseEvents();
