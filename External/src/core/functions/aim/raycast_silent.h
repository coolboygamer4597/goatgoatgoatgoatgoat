#pragma once

#include "../../../sdk/structs.h"
#include <cstdint>

namespace Cheat::Features::RaycastSilent {

bool Install();
void Remove();
void Ensure(bool want = true);
void SetActive(bool on, const Vector3& world_target = {}, bool wallbang = false);
bool Ready();
bool Aiming();
bool WallbangMode();
std::uintptr_t OriginalHandler();
std::uint64_t WorldCalls();
std::uint64_t LegacyCalls();
std::uint64_t HandlerCalls();

}
