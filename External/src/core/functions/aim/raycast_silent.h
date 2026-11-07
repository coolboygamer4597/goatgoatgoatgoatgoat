#pragma once

#include "../../../sdk/math.h"
#include <cstdint>
#include <vector>

namespace Cheat::Features::RaycastSilent {

bool Install();
void Remove();
void Ensure(bool want = true);
void SetActive(bool on, const rbx::vector3_t& world_target = {}, bool wallbang = false, float chance = 100);
std::uint64_t WorldCalls();
std::uint64_t HandlerCalls();

}

