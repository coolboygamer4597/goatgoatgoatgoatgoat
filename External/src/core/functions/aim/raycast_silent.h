#pragma once

#include "../../../sdk/structs.h"
#include <cstdint>

namespace Cheat::Features::RaycastSilent {

bool Install();
void Remove();
void Ensure(bool want = true);
void SetActive(bool on, const Vector3& world_target = {}, bool wallbang = false);
std::uint64_t WorldCalls();
std::uint64_t HandlerCalls();

}
