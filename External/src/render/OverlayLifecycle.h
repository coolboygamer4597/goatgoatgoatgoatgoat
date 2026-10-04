#pragma once
#include <Windows.h>
#include <d3d11.h>

namespace OverlayLifecycle {
inline bool PumpMessages() {
    MSG message;
    bool running = true;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) { running = false; continue; }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return running;
}

inline HRESULT ResizeBackBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
    IDXGISwapChain* swap, ID3D11RenderTargetView*& target, UINT width, UINT height,
    UINT& bufferWidth, UINT& bufferHeight) {
    if (!device || !context || !swap || !width || !height) return E_INVALIDARG;
    if (target && width == bufferWidth && height == bufferHeight) return S_FALSE;

    context->ClearState();
    if (target) { target->Release(); target = nullptr; }
    const HRESULT resized = swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* buffer = nullptr;
    HRESULT result = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buffer));
    if (SUCCEEDED(result)) {
        D3D11_TEXTURE2D_DESC desc{};
        buffer->GetDesc(&desc);
        result = device->CreateRenderTargetView(buffer, nullptr, &target);
        buffer->Release();
        if (SUCCEEDED(result)) { bufferWidth = desc.Width; bufferHeight = desc.Height; }
    }

    return FAILED(resized) ? resized : result;
}

inline HRESULT Present(IDXGISwapChain* swap, bool vsync) {
    return swap ? swap->Present(vsync ? 1 : 0, DXGI_PRESENT_DO_NOT_WAIT) : E_INVALIDARG;
}
inline bool DeviceLost(HRESULT result) {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET
        || result == DXGI_ERROR_DEVICE_HUNG;
}
}
