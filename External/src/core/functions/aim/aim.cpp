#include "../../keys/activation_key.h"
#include "../../keys/keys.h"
#include "aim.h"
#include "../../cache/cache.h"
#include "../../../sdk/w2s.h"
#include "raycast_silent.h"
#include "silent_fov_center.h"
#include "silent_nearest_point.h"
#include <vector>
#include "../../globals/globals.h"
#include "../../../memory/memory.h"
#include "../../../sdk/offsets.h"
#include "../../../../ext/imgui/imgui.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace Aimbot {

float GetDistance2D(const RBX::Vec2& a, const RBX::Vec2& b) {
    const float dx = a.X - b.X;
    const float dy = a.Y - b.Y;
    return sqrtf(dx * dx + dy * dy);
}

void CollectHitboxParts(std::uintptr_t characterAddr, int hitbox, std::vector<std::uintptr_t>& out) {
    if (!characterAddr)
        return;
    RBX::RbxInstance ch{characterAddr};
    auto addFallback = [&](std::initializer_list<const char*> names) {
        for (const char* n : names) {
            auto part = ch.FindChild(n);
            if (part.Addr) {
                out.push_back(part.Addr);
                return;
            }
        }
    };
    switch (hitbox) {
    case 0: addFallback({"Head"}); break;
    case 1: addFallback({"Torso", "UpperTorso", "LowerTorso"}); break;
    case 2: addFallback({"Left Arm", "LeftUpperArm", "LeftLowerArm", "LeftHand"}); break;
    case 3: addFallback({"Right Arm", "RightUpperArm", "RightLowerArm", "RightHand"}); break;
    case 4: addFallback({"Left Leg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot"}); break;
    case 5: addFallback({"Right Leg", "RightUpperLeg", "RightLowerLeg", "RightFoot"}); break;
    case 6: addFallback({"HumanoidRootPart"}); break;
    default: break;
    }
    if (hitbox == 7 || out.empty()) {
        if (hitbox != 7)
            out.clear();
        std::vector<std::uintptr_t> stack{characterAddr};
        while (!stack.empty() && out.size() < 64) {
            const uintptr_t cur = stack.back();
            stack.pop_back();
            const uintptr_t prim = memory->read<uintptr_t>(cur + Offsets::BasePart::Primitive);
            if (prim && prim != 0xFFFFFFFFFFFFFFFFull && prim > 0x10000 && prim < 0x7FFFFFFF0000ull)
                out.push_back(cur);
            const uintptr_t start = memory->read<uintptr_t>(cur + Offsets::Instance::ChildrenStart);
            if (!start)
                continue;
            const uintptr_t end = memory->read<uintptr_t>(start + Offsets::Instance::ChildrenEnd);
            uintptr_t it = memory->read<uintptr_t>(start);
            if (!end || !it || end < it)
                continue;
            for (uintptr_t p = it, n = 0; p < end && n < 64; p += 0x10, ++n) {
                const uintptr_t child = memory->read<uintptr_t>(p);
                if (child)
                    stack.push_back(child);
            }
        }
    }
}

RBX::Vec3 PartWorldPos(std::uintptr_t partAddr) {
    if (!partAddr)
        return {};
    const uintptr_t prim = memory->read<uintptr_t>(partAddr + Offsets::BasePart::Primitive);
    if (!prim)
        return {};
    return memory->read<RBX::Vec3>(prim + Offsets::Primitive::Position);
}

namespace {

bool IsAimPartVisible(std::uintptr_t part) {
    if (!variables::Aimbot::invisibleCheck)
        return true;
    const float transparency = memory->read<float>(part + Offsets::BasePart::Transparency);
    return std::isfinite(transparency) && transparency < 0.50f;
}

struct AimProjectedVertex {
    RBX::Vec2 screen{};
    RBX::Vec3 world{};
    bool valid = false;
};

float DistanceSq2D(const RBX::Vec2& a, const RBX::Vec2& b) {
    const float dx = a.X - b.X;
    const float dy = a.Y - b.Y;
    return dx * dx + dy * dy;
}

RBX::Vec3 BlendWorld(const AimProjectedVertex& a, const AimProjectedVertex& b,
                     const AimProjectedVertex& c, float wa, float wb, float wc) {
    return {
        a.world.X * wa + b.world.X * wb + c.world.X * wc,
        a.world.Y * wa + b.world.Y * wb + c.world.Y * wc,
        a.world.Z * wa + b.world.Z * wb + c.world.Z * wc
    };
}

bool ClosestOnProjectedTriangle(const RBX::Vec2& cursor,
                                const AimProjectedVertex& a,
                                const AimProjectedVertex& b,
                                const AimProjectedVertex& c,
                                RBX::Vec2& outScreen, RBX::Vec3& outWorld,
                                float& outDistanceSq) {
    if (!a.valid || !b.valid || !c.valid)
        return false;

    const float denominator =
        (b.screen.Y - c.screen.Y) * (a.screen.X - c.screen.X) +
        (c.screen.X - b.screen.X) * (a.screen.Y - c.screen.Y);
    if (!std::isfinite(denominator) || std::fabs(denominator) < 0.0001f)
        return false;

    const float wa =
        ((b.screen.Y - c.screen.Y) * (cursor.X - c.screen.X) +
         (c.screen.X - b.screen.X) * (cursor.Y - c.screen.Y)) / denominator;
    const float wb =
        ((c.screen.Y - a.screen.Y) * (cursor.X - c.screen.X) +
         (a.screen.X - c.screen.X) * (cursor.Y - c.screen.Y)) / denominator;
    const float wc = 1.0f - wa - wb;

    float bestSq = FLT_MAX;
    RBX::Vec2 bestScreen{};
    RBX::Vec3 bestWorld{};
    bool found = false;

    if (wa >= -0.0001f && wb >= -0.0001f && wc >= -0.0001f) {
        bestSq = 0.0f;
        bestScreen = cursor;
        bestWorld = BlendWorld(a, b, c, wa, wb, wc);
        found = true;
    } else {
        auto testEdge = [&](const AimProjectedVertex& p0,
                            const AimProjectedVertex& p1) {
            const float ex = p1.screen.X - p0.screen.X;
            const float ey = p1.screen.Y - p0.screen.Y;
            const float lengthSq = ex * ex + ey * ey;
            if (!std::isfinite(lengthSq) || lengthSq < 0.0001f)
                return;
            float t = ((cursor.X - p0.screen.X) * ex +
                       (cursor.Y - p0.screen.Y) * ey) / lengthSq;
            t = (std::max)(0.0f, (std::min)(1.0f, t));
            const RBX::Vec2 point{
                p0.screen.X + ex * t,
                p0.screen.Y + ey * t
            };
            const float distanceSq = DistanceSq2D(cursor, point);
            if (distanceSq >= bestSq)
                return;
            bestSq = distanceSq;
            bestScreen = point;
            bestWorld = {
                p0.world.X + (p1.world.X - p0.world.X) * t,
                p0.world.Y + (p1.world.Y - p0.world.Y) * t,
                p0.world.Z + (p1.world.Z - p0.world.Z) * t
            };
            found = true;
        };
        testEdge(a, b);
        testEdge(b, c);
        testEdge(c, a);
    }

    if (!found)
        return false;
    outScreen = bestScreen;
    outWorld = bestWorld;
    outDistanceSq = bestSq;
    return true;
}

bool ClosestPointOnR6Part(std::uintptr_t part, const RBX::Mat4& view,
                          const RBX::Vec2& cursor, float maxDistance,
                          RBX::Vec2& outScreen, RBX::Vec3& outWorld,
                          float& outDistance) {
    const std::uintptr_t primitive = part
        ? memory->read<std::uintptr_t>(part + Offsets::BasePart::Primitive) : 0;
    if (!primitive)
        return false;

    const RBX::CFrame frame = memory->read<RBX::CFrame>(
        primitive + Offsets::Primitive::Rotation);
    const RBX::Vec3 center = memory->read<RBX::Vec3>(
        primitive + Offsets::Primitive::Position);
    const RBX::Vec3 size = memory->read<RBX::Vec3>(
        primitive + Offsets::Primitive::Size);
    if (!std::isfinite(center.X) || !std::isfinite(center.Y) || !std::isfinite(center.Z) ||
        !std::isfinite(size.X) || !std::isfinite(size.Y) || !std::isfinite(size.Z) ||
        size.X < 0.05f || size.Y < 0.05f || size.Z < 0.05f ||
        size.X > 20.0f || size.Y > 20.0f || size.Z > 20.0f)
        return false;

    const RBX::Vec3 right = frame.GetRightVector();
    const RBX::Vec3 up = frame.GetUpVector();
    const RBX::Vec3 back = frame.GetLookVector();
    const float hx = size.X * 0.5f;
    const float hy = size.Y * 0.5f;
    const float hz = size.Z * 0.5f;
    AimProjectedVertex vertices[8]{};
    for (int i = 0; i < 8; ++i) {
        const float lx = (i & 1) ? hx : -hx;
        const float ly = (i & 2) ? hy : -hy;
        const float lz = (i & 4) ? hz : -hz;
        AimProjectedVertex& vertex = vertices[i];
        vertex.world = {
            center.X + right.X * lx + up.X * ly + back.X * lz,
            center.Y + right.Y * lx + up.Y * ly + back.Y * lz,
            center.Z + right.Z * lx + up.Z * ly + back.Z * lz
        };
        vertex.screen = W2S::WorldToScreen(vertex.world, view);
        vertex.valid = std::isfinite(vertex.screen.X) && std::isfinite(vertex.screen.Y) &&
            !(vertex.screen.X == 0.0f && vertex.screen.Y == 0.0f);
    }

    static constexpr int triangles[12][3] = {
        {0,1,3}, {0,3,2}, {4,6,7}, {4,7,5},
        {0,4,5}, {0,5,1}, {2,3,7}, {2,7,6},
        {0,2,6}, {0,6,4}, {1,5,7}, {1,7,3}
    };
    float bestSq = maxDistance * maxDistance;
    bool found = false;
    for (const auto& triangle : triangles) {
        RBX::Vec2 screen{};
        RBX::Vec3 world{};
        float distanceSq = FLT_MAX;
        if (!ClosestOnProjectedTriangle(cursor,
                vertices[triangle[0]], vertices[triangle[1]], vertices[triangle[2]],
                screen, world, distanceSq) || distanceSq >= bestSq)
            continue;
        bestSq = distanceSq;
        outScreen = screen;

        outWorld = {
            world.X * 0.90f + center.X * 0.10f,
            world.Y * 0.90f + center.Y * 0.10f,
            world.Z * 0.90f + center.Z * 0.10f
        };
        found = true;
    }
    if (found)
        outDistance = std::sqrt(bestSq);
    return found;
}

bool BestPartScreen(const PlayerCache::CachedPlayer& plr, const RBX::Mat4& view, const RBX::Vec2& center, float maxDist, RBX::Vec2& outScreen, RBX::Vec3& outWorld) {
    const float sw = W2S::ScreenW();
    const float sh = W2S::ScreenH();
    const int mode = variables::Aimbot::aimTarget;
    if (mode != 7) {
        std::uintptr_t addr = 0;
        if (mode == 0)
            addr = plr.headAddr;
        else if (mode == 6)
            addr = plr.rootPartAddr;
        else {
            const PlayerCache::LimbAddrs& limbs = PlayerCache::GetLimbs(plr.characterAddr);
            if (mode == 1)
                addr = limbs.r6 ? limbs.torso : limbs.upperTorso;
            else if (mode == 2)
                addr = limbs.r6 ? limbs.lArm : limbs.lUpperArm;
            else if (mode == 3)
                addr = limbs.r6 ? limbs.rArm : limbs.rUpperArm;
            else if (mode == 4)
                addr = limbs.r6 ? limbs.lLeg : limbs.lUpperLeg;
            else if (mode == 5)
                addr = limbs.r6 ? limbs.rLeg : limbs.rUpperLeg;
        }
        if (!addr)
            return false;
        if (!IsAimPartVisible(addr))
            return false;
        const RBX::Vec3 w = PartWorldPos(addr);
        if (w.X == 0 && w.Y == 0 && w.Z == 0)
            return false;
        const RBX::Vec2 s = W2S::WorldToScreen(w, view);
        if (s.X == 0 && s.Y == 0)
            return false;
        if (s.X < 0 || s.Y < 0 || s.X > sw || s.Y > sh)
            return false;
        if (GetDistance2D(center, s) >= maxDist)
            return false;
        outScreen = s;
        outWorld = w;
        return true;
    }
    std::vector<std::uintptr_t> parts;
    const PlayerCache::LimbAddrs& limbs = PlayerCache::GetLimbs(plr.characterAddr);
    if (limbs.r6) {
        const std::uintptr_t r6Parts[] = {
            limbs.head, limbs.torso, limbs.lArm,
            limbs.rArm, limbs.lLeg, limbs.rLeg
        };
        for (const std::uintptr_t part : r6Parts) {
            if (part)
                parts.push_back(part);
        }
    } else {
        CollectHitboxParts(plr.characterAddr, 7, parts);
    }
    if (parts.empty())
        return false;
    float best = maxDist;
    bool found = false;
    for (auto addr : parts) {
        if (!IsAimPartVisible(addr))
            continue;
        if (limbs.r6) {
            RBX::Vec2 preciseScreen{};
            RBX::Vec3 preciseWorld{};
            float preciseDistance = FLT_MAX;
            if (!ClosestPointOnR6Part(addr, view, center, best,
                    preciseScreen, preciseWorld, preciseDistance))
                continue;
            best = preciseDistance;
            outScreen = preciseScreen;
            outWorld = preciseWorld;
            found = true;
            continue;
        }
        const RBX::Vec3 w = PartWorldPos(addr);
        if (w.X == 0 && w.Y == 0 && w.Z == 0)
            continue;
        const RBX::Vec2 s = W2S::WorldToScreen(w, view);
        if (s.X == 0 && s.Y == 0)
            continue;
        if (s.X < 0 || s.Y < 0 || s.X > sw || s.Y > sh)
            continue;
        const float d = GetDistance2D(center, s);
        if (d >= best)
            continue;
        best = d;
        outScreen = s;
        outWorld = w;
        found = true;
    }
    return found;
}
}

void RunAimbot(const RBX::Mat4& view) {
    const bool enabled = variables::Aimbot::silentEnabled;
    static Keys::ActivationKey activation;
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    const bool keyActive = activation.Update(enabled, variables::Aimbot::silentToggleKey,
        variables::Aimbot::silentKey, Keys::IsKeyPressed(variables::Aimbot::silentKey),
        !variables::menuOpen && foregroundPid == memory->get_process_id());
    variables::Aimbot::silentActive = keyActive;
    const bool magicBullet = variables::Aimbot::silentMethod == 2;
    static bool loggedTarget = false;
    static bool loggedHandlerActivity = false;
    static bool loggedRayActivity = false;

    if (!enabled) {
        loggedTarget = false;
        loggedHandlerActivity = false;
        loggedRayActivity = false;

        Cheat::Features::RaycastSilent::SetActive(false);
        Cheat::Features::RaycastSilent::Ensure(false);
        return;
    }

    Cheat::Features::RaycastSilent::Ensure(true);

    if (!keyActive) {

        Cheat::Features::RaycastSilent::SetActive(false);
        return;
    }

    const RBX::Vec2 center = SilentAimCenter();
    const float radius = SilentAimRadius();
    const PlayerCache::CachedPlayer* bestPlayer = nullptr;
    RBX::Vec3 bestWorld{};
    float bestScore = FLT_MAX;

    const bool nearestPoint = variables::Aimbot::silentNearestPoint;
    for (const auto& player : PlayerCache::players) {
        if (!player.isValid || PlayerRules::Aim(player.userId))
            continue;
        if (variables::Aimbot::deadCheck && player.health <= 0.0f)
            continue;
        if (variables::Aimbot::teamCheck && player.teamAddr &&
            player.teamAddr == PlayerCache::localPlayerTeam)
            continue;

        RBX::Vec2 screen{};
        RBX::Vec3 world{};
        if (nearestPoint) {

            std::vector<std::uintptr_t> parts;
            const PlayerCache::LimbAddrs& limbs =
                PlayerCache::GetLimbs(player.characterAddr);
            if (limbs.r6) {
                const std::uintptr_t r6Parts[] = {
                    limbs.head, limbs.torso, limbs.lArm,
                    limbs.rArm, limbs.lLeg, limbs.rLeg
                };
                for (const std::uintptr_t part : r6Parts)
                    if (part)
                        parts.push_back(part);
            } else {
                CollectHitboxParts(player.characterAddr, 7, parts);
            }
            const PrecisePointResult point =
                ResolveNearestPoint(parts, view, center, radius);
            if (!point.found)
                continue;
            screen = point.screen;
            world = point.world;
        } else if (!BestPartScreen(player, view, center, radius, screen, world)) {
            continue;
        }

        const float score = GetDistance2D(center, screen);

        if (score < bestScore) {
            bestScore = score;
            bestPlayer = &player;
            bestWorld = world;
        }
    }

    if (!bestPlayer) {

        Cheat::Features::RaycastSilent::SetActive(false);
        return;
    }

    const Vector3 rayTarget{bestWorld.X, bestWorld.Y, bestWorld.Z};
    Cheat::Features::RaycastSilent::SetActive(true, rayTarget, magicBullet);
    if (!loggedTarget) {
        std::printf("[Silent] target armed method=%s part=0x%llx\n",
            magicBullet ? "magic-raycast" : "raycast",
            static_cast<unsigned long long>(bestPlayer->playerAddr));
        loggedTarget = true;
    }
    if (!loggedHandlerActivity) {
        const std::uint64_t handlerCalls = Cheat::Features::RaycastSilent::HandlerCalls();
        if (handlerCalls) {
            std::printf("[Silent] ray handler observed calls=%llu\n",
                static_cast<unsigned long long>(handlerCalls));
            loggedHandlerActivity = true;
        }
    }
    if (!loggedRayActivity) {
        const std::uint64_t redirectedCalls = Cheat::Features::RaycastSilent::WorldCalls();
        if (redirectedCalls) {
            std::printf("[Silent] ray redirected calls=%llu\n",
                static_cast<unsigned long long>(redirectedCalls));
            loggedRayActivity = true;
        }
    }
}
}
