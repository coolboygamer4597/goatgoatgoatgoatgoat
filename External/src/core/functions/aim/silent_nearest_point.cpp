#include "silent_nearest_point.h"
#include "../../cache/workspace.h"
#include "../../cache/worldcache.h"
#include "../../globals/globals.h"
#include "../../variables/variables.h"
#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include "../../../sdk/w2s.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace Aimbot {
namespace {
bool PartVisible(std::uintptr_t part) {
    if (!variables::Aimbot::invisibleCheck)
        return true;
    const float transparency = memory->read<float>(part + Offsets::BasePart::Transparency);
    return std::isfinite(transparency) && transparency < 0.50f;
}

float Dist2D(const RBX::Vec2& a, const RBX::Vec2& b) {
    const float dx = a.X - b.X;
    const float dy = a.Y - b.Y;
    return std::sqrt(dx * dx + dy * dy);
}
}

PrecisePointResult ResolveNearestPoint(const std::vector<std::uintptr_t>& partAddrs,
                                       const RBX::Mat4& view,
                                       const RBX::Vec2& center,
                                       float radius) {
    PrecisePointResult out;
    if (partAddrs.empty())
        return out;

    RBX::Vec3 cam{};
    if (Globals::camera.Addr)
        cam = memory->read<RBX::Vec3>(Globals::camera.Addr + Offsets::Camera::Position);

    float best = radius;
    bool found = false;

    for (const std::uintptr_t part : partAddrs) {
        if (!part || !PartVisible(part))
            continue;
        const std::uintptr_t prim =
            memory->read<std::uintptr_t>(part + Offsets::BasePart::Primitive);
        if (!prim)
            continue;
        const RBX::CFrame frame =
            memory->read<RBX::CFrame>(prim + Offsets::Primitive::Rotation);
        const RBX::Vec3 centerPos =
            memory->read<RBX::Vec3>(prim + Offsets::Primitive::Position);
        const RBX::Vec3 size = memory->read<RBX::Vec3>(prim + Offsets::Primitive::Size);
        if (!std::isfinite(centerPos.X) || !std::isfinite(centerPos.Y) || !std::isfinite(centerPos.Z) ||
            !std::isfinite(size.X) || !std::isfinite(size.Y) || !std::isfinite(size.Z) ||
            size.X < 0.05f || size.Y < 0.05f || size.Z < 0.05f ||
            size.X > 20.0f || size.Y > 20.0f || size.Z > 20.0f)
            continue;

        const RBX::Vec3 axes[3] = {
            frame.GetRightVector(),
            frame.GetUpVector(),
            frame.GetLookVector()
        };
        const float half[3] = {size.X * 0.5f, size.Y * 0.5f, size.Z * 0.5f};

        int nearAxis = 0;
        float nearSign = 1.0f;
        float nearDist = FLT_MAX;
        for (int axis = 0; axis < 3; ++axis) {
            for (const float sign : {1.0f, -1.0f}) {
                const RBX::Vec3 faceCenter{
                    centerPos.X + axes[axis].X * half[axis] * sign,
                    centerPos.Y + axes[axis].Y * half[axis] * sign,
                    centerPos.Z + axes[axis].Z * half[axis] * sign
                };
                const float dx = cam.X - faceCenter.X;
                const float dy = cam.Y - faceCenter.Y;
                const float dz = cam.Z - faceCenter.Z;
                const float d = dx * dx + dy * dy + dz * dz;
                if (d < nearDist) {
                    nearDist = d;
                    nearAxis = axis;
                    nearSign = sign;
                }
            }
        }

        const int t1 = (nearAxis + 1) % 3;
        const int t2 = (nearAxis + 2) % 3;
        const RBX::Vec3 faceCenter{
            centerPos.X + axes[nearAxis].X * half[nearAxis] * nearSign,
            centerPos.Y + axes[nearAxis].Y * half[nearAxis] * nearSign,
            centerPos.Z + axes[nearAxis].Z * half[nearAxis] * nearSign
        };

        const float fracs[4] = {-0.75f, -0.25f, 0.25f, 0.75f};
        for (const float fa : fracs) {
            for (const float fb : fracs) {
                RBX::Vec3 world{
                    faceCenter.X + axes[t1].X * half[t1] * fa + axes[t2].X * half[t2] * fb,
                    faceCenter.Y + axes[t1].Y * half[t1] * fa + axes[t2].Y * half[t2] * fb,
                    faceCenter.Z + axes[t1].Z * half[t1] * fa + axes[t2].Z * half[t2] * fb
                };
                world = {
                    world.X * 0.90f + centerPos.X * 0.10f,
                    world.Y * 0.90f + centerPos.Y * 0.10f,
                    world.Z * 0.90f + centerPos.Z * 0.10f
                };
                const RBX::Vec2 s = W2S::WorldToScreen(world, view);
                if (!std::isfinite(s.X) || !std::isfinite(s.Y))
                    continue;
                if (s.X == 0.0f && s.Y == 0.0f)
                    continue;
                const float d = Dist2D(center, s);
                if (d < best) {
                    best = d;
                    out.screen = s;
                    out.world = world;
                    out.distance = d;
                    found = true;
                }
            }
        }
    }

    out.found = found;
    return out;
}
}
