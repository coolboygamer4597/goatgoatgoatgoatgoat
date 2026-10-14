#pragma once

#include "../../../sdk/sdk.h"
#include "../../../../ext/imgui/imgui.h"

namespace Aimbot {

RBX::Vec2 SilentAimCenter();

float SilentAimRadius();

inline void DrawFovCircle(ImDrawList* list, const ImVec2& center, float radius, ImU32 color) {
    if (!list || !std::isfinite(radius) || radius < 1.f || !(color & IM_COL32_A_MASK)) return;
    const auto flags = list->Flags;
    list->Flags = (flags | ImDrawListFlags_AntiAliasedLines) & ~ImDrawListFlags_AntiAliasedLinesUseTex;
    const int segments = std::clamp(static_cast<int>(std::ceil(3.141592654f /
        std::acos(1.f - .08f / (std::min)(radius, 10000.f)))), 48, 384);
    const unsigned alpha = (color >> IM_COL32_A_SHIFT) & 255;
    list->AddCircle(center, radius, IM_COL32(8, 8, 8, alpha * 2 / 3), segments, 2.5f);
    list->AddCircle(center, radius, color, segments, 1.25f);
    list->Flags = flags;
}
}
