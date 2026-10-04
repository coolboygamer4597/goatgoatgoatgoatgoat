#pragma once

#include "sdk/MeshMath.h"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Cheat::Visuals::MeshOcclusion {

void Reset();

bool PartFaded(std::uint64_t partAddress);
std::size_t FadeOffset();

float PartFadeValue(std::uint64_t partAddress);

void VisitOccluders(
    const Mesh::Vector3& camera,
    float maxDistance,
    std::size_t maxCount,
    const std::function<void(const Mesh::Vector3&, const Mesh::Matrix4x4&, const Mesh::Vector3&,
                             const std::string&, std::uint8_t, bool, bool)>& visitor);

struct Relevance {
    Mesh::Vector3 from{};
    Mesh::Vector3 to{};
    float radius = 0.0f;
};

void VisitOccluders(
    const Mesh::Vector3& camera,
    float maxDistance,
    std::size_t maxCount,
    const std::vector<Relevance>& relevances,
    const std::function<void(const Mesh::Vector3&, const Mesh::Matrix4x4&, const Mesh::Vector3&,
                             const std::string&, std::uint8_t, bool, bool)>& visitor);

}
