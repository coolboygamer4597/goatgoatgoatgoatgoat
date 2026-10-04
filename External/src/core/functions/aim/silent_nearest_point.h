#pragma once

#include "../../../sdk/sdk.h"
#include <cstdint>
#include <vector>

namespace Aimbot {
struct PrecisePointResult {
    bool found = false;
    RBX::Vec2 screen{};
    RBX::Vec3 world{};
    float distance = 0.f;
};

PrecisePointResult ResolveNearestPoint(const std::vector<std::uintptr_t>& partAddrs,
                                       const RBX::Mat4& view,
                                       const RBX::Vec2& center,
                                       float radius);
}
