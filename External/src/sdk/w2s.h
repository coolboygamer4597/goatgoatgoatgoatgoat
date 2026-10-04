#pragma once
#include "sdk.h"

namespace W2S {
inline RBX::Vec2 ViewportSize() {
    static RBX::Vec2 size{
        static_cast<float>(GetSystemMetrics(SM_CXSCREEN)),
        static_cast<float>(GetSystemMetrics(SM_CYSCREEN))};
    static ULONGLONG lastRefresh = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - lastRefresh >= 100) {
        lastRefresh = now;
        const HWND game = FindWindowW(nullptr, L"Roblox");
        RECT client{};
        if (game && GetClientRect(game, &client)) {
            const float width = static_cast<float>(client.right - client.left);
            const float height = static_cast<float>(client.bottom - client.top);
            if (width >= 64.0f && height >= 64.0f)
                size = {width, height};
        }
    }
    return size;
}

inline float ScreenW() {
    return ViewportSize().X;
}

inline float ScreenH() {
    return ViewportSize().Y;
}

inline RBX::Vec2 WorldToScreen(const RBX::Vec3& world, const RBX::Mat4& view) {
    RBX::Vec2 screen{};
    const float w = world.X * view.data[12] + world.Y * view.data[13] + world.Z * view.data[14] + view.data[15];
    if (w < 0.1f)
        return screen;
    const float x = world.X * view.data[0] + world.Y * view.data[1] + world.Z * view.data[2] + view.data[3];
    const float y = world.X * view.data[4] + world.Y * view.data[5] + world.Z * view.data[6] + view.data[7];
    const float sw = ScreenW();
    const float sh = ScreenH();
    screen.X = (sw * 0.5f * (x / w)) + (sw * 0.5f);
    screen.Y = -(sh * 0.5f * (y / w)) + (sh * 0.5f);
    return screen;
}
}
