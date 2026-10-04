#pragma once
#include <cstdint>
#include <string>

namespace App {
bool WithinVisualRange(std::uintptr_t character,float limit,bool unlimited);
bool PassesVisibilityChecks(std::uintptr_t character);
bool PassesChamChecks(bool valid, std::uintptr_t character, std::uintptr_t team, float health);
bool game_open();
bool init();
std::wstring StartupError();
std::int32_t Run();
}
