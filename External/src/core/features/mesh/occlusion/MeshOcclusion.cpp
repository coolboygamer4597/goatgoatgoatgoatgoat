#include "MeshOcclusion.h"
#include "AvatarOccluderFilter.h"
#include "sdk/MeshBridge.h"
#include "core/globals/globals.h"
#include "core/cache/cache.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

#include "sdk/offsets.h"

namespace Cheat::Visuals::MeshOcclusion {
namespace {

struct Rotation3 {
    float m[9]{};
};

constexpr float kInvisibleAlpha = 0.95f;
std::size_t g_ltmOffset = 0;
bool PartHiddenByAlpha(std::uint64_t part) {
    if (!g_Memory.IsValid(part))
        return false;
    const float transparency = g_Memory.Read<float>(part + Offsets::BasePart::Transparency);
    if (std::isfinite(transparency) && transparency >= kInvisibleAlpha)
        return true;
    if (g_ltmOffset != 0) {
        const float ltm = g_Memory.Read<float>(part + g_ltmOffset);
        if (std::isfinite(ltm) && ltm >= kInvisibleAlpha)
            return true;
    }
    return false;
}

struct Occluder {
    std::uint64_t primitive{};
    Mesh::Vector3 position{};
    Mesh::Vector3 size{};
    Rotation3 rotation{};
    std::string mesh_id{};
    std::uint8_t shape{ 0 };
    bool can_query{ true };
};

struct Cache {
    std::vector<Occluder> parts;
    std::vector<Occluder> buildingParts;
    std::unordered_set<std::uint64_t> ignored;
    std::uint64_t workspace{};
    std::vector<std::uint64_t> pending;
    std::unordered_set<std::uint64_t> visited;
    std::unordered_set<std::uint64_t> ignoredRoots;
    std::size_t cursor{};
    bool building{};
    ULONGLONG completedAt{};
    ULONGLONG lastStep{};
};

Cache& State() {
    static Cache state;
    return state;
}

bool g_ltmTried = false;
void CalibrateLtmOffset(Cache& cache) {
    if (g_ltmTried || cache.buildingParts.size() < 8)
        return;
    g_ltmTried = true;
    constexpr std::size_t kCandidates[] = { 0x134, 0x138, 0x13C, 0x140, 0x148 };
    for (const std::size_t cand : kCandidates) {
        std::size_t zeroish = 0, total = 0;
        for (std::size_t i = 0; i < cache.buildingParts.size() && total < 48; ++i) {
            const std::uint64_t owner = g_Memory.Read<std::uint64_t>(
                cache.buildingParts[i].primitive + Offsets::Primitive::Part);
            if (!g_Memory.IsValid(owner))
                continue;
            ++total;
            const float v = g_Memory.Read<float>(owner + cand);
            if (std::isfinite(v) && v >= 0.0f && v <= 1.0f) {
                if (v <= 0.001f)
                    ++zeroish;
            } else {
                total = 0;
                break;
            }
        }
        if (total >= 24 && zeroish >= total - total / 8) {
            g_ltmOffset = cand;
            break;
        }
    }
}

bool Finite(const Mesh::Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsVisualOccluder(std::uint64_t primitive) {

    const std::uint64_t part = g_Memory.Read<std::uint64_t>(
        primitive + Offsets::Primitive::Part);
    if (g_Memory.IsValid(part))
        return !PartHiddenByAlpha(part);

    const std::uint8_t flags = g_Memory.Read<std::uint8_t>(
        primitive + Offsets::Primitive::Flags);
    return (flags & static_cast<std::uint8_t>(Offsets::PrimitiveFlags::CanCollide)) != 0;
}

std::string ReadOccluderMeshId(std::uint64_t part, const std::string& cls, std::uint8_t& shape) {
    shape = 0;
    if (!g_Memory.IsValid(part))
        return {};
    if (cls == "MeshPart") {
        std::string s = g_Memory.ReadString(part + Offsets::MeshPart::MeshId);
        if (s.empty() || s == "Unknown") {
            const std::uint64_t p = g_Memory.Read<std::uint64_t>(part + Offsets::MeshPart::MeshId);
            if (g_Memory.IsValid(p)) s = g_Memory.ReadString(p);
        }
        if (s == "Unknown") s.clear();
        return s;
    }
    if (cls != "Part" && cls != "WedgePart" && cls != "CornerWedgePart" &&
        cls != "UnionOperation")
        return {};

    for (const auto& child : RBX::RbxInstance(part).GetChildList()) {
        const std::string ccls = child.GetClass();
        if (ccls == "CylinderMesh") {
            shape = 1;
            return {};
        }
        if (ccls == "SpecialMesh" || ccls == "FileMesh" ||
            ccls == "BlockMesh") {
            std::string s = g_Memory.ReadString(child.Addr + Offsets::SpecialMesh::MeshId);
            if (s.empty() || s == "Unknown") {
                const std::uint64_t p = g_Memory.Read<std::uint64_t>(child.Addr + Offsets::SpecialMesh::MeshId);
                if (g_Memory.IsValid(p)) s = g_Memory.ReadString(p);
            }
            if (s != "Unknown" && !s.empty()) return s;
            return {};
        }
    }

    if ((cls == "WedgePart" || cls == "CornerWedgePart") && shape == 0)
        shape = 2;
    return {};
}

float DistanceToBoundsSquared(const Occluder& part, const Mesh::Vector3& point) {
    const float hx = part.size.x * 0.5f;
    const float hy = part.size.y * 0.5f;
    const float hz = part.size.z * 0.5f;

    const Mesh::Vector3 extent{
        std::fabs(part.rotation.m[0]) * hx + std::fabs(part.rotation.m[1]) * hy + std::fabs(part.rotation.m[2]) * hz,
        std::fabs(part.rotation.m[3]) * hx + std::fabs(part.rotation.m[4]) * hy + std::fabs(part.rotation.m[5]) * hz,
        std::fabs(part.rotation.m[6]) * hx + std::fabs(part.rotation.m[7]) * hy + std::fabs(part.rotation.m[8]) * hz
    };
    const Mesh::Vector3 delta = part.position - point;
    const float qx = (std::max)(std::fabs(delta.x) - extent.x, 0.0f);
    const float qy = (std::max)(std::fabs(delta.y) - extent.y, 0.0f);
    const float qz = (std::max)(std::fabs(delta.z) - extent.z, 0.0f);
    return qx * qx + qy * qy + qz * qz;
}

void IgnoreCharacter(std::uint64_t character, std::unordered_set<std::uint64_t>& ignored) {

    constexpr int kMaxDepth = 4;
    struct Node { std::uint64_t addr; int depth; };
    std::vector<Node> stack;
    if (character)
        stack.push_back({character, 0});
    while (!stack.empty()) {
        const Node node = stack.back();
        stack.pop_back();
        if (node.depth >= kMaxDepth)
            continue;
        for (const auto& child : RBX::RbxInstance(node.addr).GetChildList()) {
            const std::string cls = child.GetClass();
            if (cls == "Part" || cls == "MeshPart" || cls == "WedgePart" ||
                cls == "CornerWedgePart" || cls == "UnionOperation" || cls == "TrussPart") {
                const std::uint64_t primitive = child.GetPrimitivePtr();
                if (primitive)
                    ignored.insert(primitive);
            } else if (child.Addr) {

                stack.push_back({child.Addr, node.depth + 1});
            }
        }
    }
}

void BeginBuild(Cache& cache, std::uint64_t workspace) {
    if (cache.workspace != workspace)
        cache.parts.clear();
    cache.buildingParts.clear();
    cache.ignored.clear();
    cache.workspace = workspace;
    cache.pending.clear();cache.visited.clear();cache.ignoredRoots.clear();
    if(workspace)cache.pending.push_back(workspace);
    cache.cursor = 0;
    cache.building = workspace != 0;
    cache.lastStep = 0;
    cache.buildingParts.reserve(8192);
    IgnoreCharacter(Globals::localPlayer.GetModelRef().Addr, cache.ignored);
    for (const auto& player : PlayerCache::players)
        IgnoreCharacter(player.characterAddr, cache.ignored);

    IgnoreCharacter(Globals::camera.Addr, cache.ignored);
    cache.ignoredRoots.insert(Globals::localPlayer.GetModelRef().Addr);
    cache.ignoredRoots.insert(Globals::camera.Addr);
    for(const auto& player:PlayerCache::players)cache.ignoredRoots.insert(player.characterAddr);
    cache.ignoredRoots.erase(0);
}

bool Append(std::uint64_t primitive, Cache& cache) {
    if (!primitive || cache.ignored.count(primitive))
        return false;
    const std::uint32_t signature = g_Memory.Read<std::uint32_t>(primitive + Offsets::Primitive::Validate);
    const bool validSignature = signature == 6 || (signature & 0xFFu) == 6 ||
        ((signature >> 8) & 0xFFu) == 6 || ((signature >> 16) & 0xFFu) == 6 ||
        ((signature >> 24) & 0xFFu) == 6;
    if (!validSignature)
        return false;
    if (!IsVisualOccluder(primitive))
        return false;
    const Mesh::Vector3 position = g_Memory.Read<Mesh::Vector3>(primitive + Offsets::Primitive::Position);
    const Mesh::Vector3 size = g_Memory.Read<Mesh::Vector3>(primitive + Offsets::Primitive::Size);
    if (!Finite(position) || !Finite(size) || size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f ||
        size.x > 500.0f || size.y > 500.0f || size.z > 500.0f)
        return false;
    Occluder part{};
    part.primitive = primitive;
    part.position = position;
    part.size = size;

    const std::uint64_t owner = g_Memory.Read<std::uint64_t>(primitive + Offsets::Primitive::Part);
    if (!g_Memory.IsValid(owner)) {

        return false;
    }
    const std::string ownerClass = RBX::RbxInstance(owner).GetClass();
    {
        const std::uint8_t pflags = g_Memory.Read<std::uint8_t>(primitive + Offsets::Primitive::Flags);
        part.can_query = (pflags & 0x20u) != 0;
        if (!part.can_query)
            return false;
        if (PartHiddenByAlpha(owner))
            return false;

        if (ownerClass == "TrussPart")
            return false;
        if (ownerClass == "UnionOperation" &&
            !variables::ESP::meshChamsUnionWalls)
            return false;
    }
    g_Memory.ReadRaw(primitive + Offsets::Primitive::Rotation, part.rotation.m, sizeof(part.rotation.m));
    for (float value : part.rotation.m) {
        if (!std::isfinite(value)) {
            part.rotation = Rotation3{{1,0,0,0,1,0,0,0,1}};
            break;
        }
    }
    part.mesh_id = ReadOccluderMeshId(owner, ownerClass, part.shape);
    cache.buildingParts.push_back(part);
    CalibrateLtmOffset(cache);
    return true;
}

void Step(Cache& cache) {
    if (!cache.building || !cache.workspace)
        return;
    const ULONGLONG now = GetTickCount64();
    if (cache.lastStep && now - cache.lastStep < 20)
        return;
    cache.lastStep = now;
    constexpr std::size_t stepCount = 2048;
    constexpr std::size_t maxScan = 65536;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
    for (std::size_t processed = 0; processed < stepCount && cache.cursor < maxScan && !cache.pending.empty(); ++processed) {
        if (processed && std::chrono::steady_clock::now() >= deadline)
            break;
        const auto addr=cache.pending.back();cache.pending.pop_back();
        if(!addr || cache.ignoredRoots.count(addr) || !cache.visited.insert(addr).second)continue;
        ++cache.cursor;
        const RBX::RbxInstance instance(addr);const auto cls=instance.GetClass();
        if(cls=="Part" || cls=="MeshPart" || cls=="UnionOperation" || cls=="WedgePart" ||
           cls=="CornerWedgePart" || cls=="TrussPart") {
            const auto primitive=instance.GetPrimitivePtr();
            if(primitive && g_Memory.Read<std::uint64_t>(primitive+Offsets::Primitive::Part)==addr)
                Append(primitive,cache);
            continue;
        }
        if(cls.empty() || cls=="Script" || cls=="LocalScript" || cls=="ModuleScript" ||
           cls=="Tool" || cls=="Accessory" || cls=="Camera")continue;

        const auto list=g_Memory.Read<std::uint64_t>(addr+Offsets::Instance::ChildrenStart);
        const auto begin=g_Memory.Read<std::uint64_t>(list);
        const auto end=g_Memory.Read<std::uint64_t>(list+Offsets::Instance::ChildrenEnd);
        if(!begin || end<begin || end-begin>4096*16 || (end-begin)%16)continue;
        std::vector<std::uint64_t> children((end-begin)/8);
        if(!children.empty() && memory->read_raw(begin,children.data(),children.size()*8))
            for(std::size_t i=0;i<children.size() && cache.pending.size()<maxScan;i+=2)
                if(children[i])cache.pending.push_back(children[i]);
    }
    if (cache.cursor >= maxScan || cache.pending.empty())
        cache.building = false;
    if (!cache.building) {
        cache.parts.swap(cache.buildingParts);
        cache.completedAt = now;
        if (g_ltmOffset == 0)
            g_ltmTried = false;
    }
}

Mesh::Matrix4x4 RotationMatrix(const Rotation3& rotation) {
    return Mesh::Matrix4x4(
        rotation.m[0], rotation.m[1], rotation.m[2], 0.0f,
        rotation.m[3], rotation.m[4], rotation.m[5], 0.0f,
        rotation.m[6], rotation.m[7], rotation.m[8], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

}

void Reset() {
    State() = Cache{};
}

Mesh::Vector3 ClosestPointOnSegment(const Mesh::Vector3& p, const Mesh::Vector3& a, const Mesh::Vector3& b) {
    const Mesh::Vector3 ab = b - a;
    const float lengthSquared = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (lengthSquared <= 1e-6f)
        return a;
    const Mesh::Vector3 ap = p - a;
    float t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / lengthSquared;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return Mesh::Vector3{ a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t };
}

bool RelevantToAny(const Occluder& part, const std::vector<Relevance>& relevances) {
    for (const Relevance& relevance : relevances) {
        const Mesh::Vector3 nearest = ClosestPointOnSegment(part.position, relevance.from, relevance.to);
        const float radius = relevance.radius > 0.0f ? relevance.radius : 0.0f;
        if (DistanceToBoundsSquared(part, nearest) <= radius * radius)
            return true;
    }
    return false;
}

void VisitOccluders(const Mesh::Vector3& camera, float maxDistance, std::size_t maxCount,
                    const std::function<void(const Mesh::Vector3&, const Mesh::Matrix4x4&, const Mesh::Vector3&,
                                             const std::string&, std::uint8_t, bool, bool)>& visitor) {
    static const std::vector<Relevance> noRelevances;
    VisitOccluders(camera, maxDistance, maxCount, noRelevances, visitor);
}

void VisitOccluders(const Mesh::Vector3& camera, float maxDistance, std::size_t maxCount,
                    const std::vector<Relevance>& relevances,
                    const std::function<void(const Mesh::Vector3&, const Mesh::Matrix4x4&, const Mesh::Vector3&,
                                             const std::string&, std::uint8_t, bool, bool)>& visitor) {
    if (!visitor || maxCount == 0 || maxDistance <= 0.0f)
        return;
    Cache& cache = State();
    const std::uint64_t root = Globals::workspace.Addr;
    const ULONGLONG now = GetTickCount64();
    if (root != cache.workspace || (!cache.building && (!cache.completedAt || now - cache.completedAt > 2000)))
        BeginBuild(cache, root);
    Step(cache);

    struct Candidate { float distanceSquared; std::size_t index; };
    std::vector<Candidate> candidates;
    const float maxDistanceSquared = maxDistance * maxDistance;
    const std::vector<Occluder>& visibleParts = cache.parts.empty() ?
        cache.buildingParts : cache.parts;
    for (std::size_t i = 0; i < visibleParts.size(); ++i) {
        const float distanceSquared = DistanceToBoundsSquared(visibleParts[i], camera);
        if (distanceSquared > maxDistanceSquared)
            continue;
        if (!relevances.empty() && !RelevantToAny(visibleParts[i], relevances))
            continue;
        candidates.push_back({distanceSquared, i});
    }
    if (candidates.size() > maxCount) {
        std::nth_element(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(maxCount),
            candidates.end(), [](const Candidate& a, const Candidate& b) {
                return a.distanceSquared < b.distanceSquared;
            });
        candidates.resize(maxCount);
    }

    AvatarOccluderFilter avatars;
    avatars.roots.insert(Globals::localPlayer.GetModelRef().Addr);
    avatars.roots.insert(Globals::camera.Addr);
    for(const auto& player:PlayerCache::players)if(player.characterAddr)avatars.roots.insert(player.characterAddr);
    avatars.roots.erase(0);
    for (const Candidate& candidate : candidates) {
        const Occluder& part = visibleParts[candidate.index];
        const float nearestGapSq = DistanceToBoundsSquared(part, camera);
        if (nearestGapSq <= 0.0009f)
            continue;

        const std::uint64_t owner = g_Memory.Read<std::uint64_t>(part.primitive + Offsets::Primitive::Part);
        if (!g_Memory.IsValid(owner) || avatars.Excludes(owner,[](std::uint64_t node) {
            return g_Memory.Read<std::uint64_t>(node+Offsets::Instance::Parent);
        }) || PartHiddenByAlpha(owner)) {
            if (visitor) visitor(part.position, RotationMatrix(part.rotation), part.size,
                                 part.mesh_id, part.shape, false, true);
            continue;
        }
        visitor(part.position, RotationMatrix(part.rotation), part.size,
                part.mesh_id, part.shape, part.can_query, false);
    }
}

bool PartFaded(std::uint64_t partAddress)
{
    return PartHiddenByAlpha(partAddress);
}

std::size_t FadeOffset()
{
    return g_ltmOffset;
}

float PartFadeValue(std::uint64_t partAddress)
{
    if (g_ltmOffset == 0 || !g_Memory.IsValid(partAddress))
        return -1.0f;
    const float value = g_Memory.Read<float>(partAddress + g_ltmOffset);
    return std::isfinite(value) ? value : -1.0f;
}

}
