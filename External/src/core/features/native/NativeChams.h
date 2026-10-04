#pragma once

#include "NativeWorldDepth.h"
#include <cstdint>
#include <string>

namespace Cheat {
namespace Visuals {
namespace NativeChams {

void Prepare();
const char* StartupStatus();
void Tick();
void Stop();
bool WorldDepthNeeded();

void SubmitWorldDepth(unsigned slot, const NativeWorldDepth::Snapshot& snapshot);

bool Hooked();

bool DrawHooked();
const char* Why();
std::uint32_t Stage();
std::uint32_t Ready();
std::uint32_t Draws();
std::uint32_t Items();
std::uint32_t Entered();
std::uint32_t Creates();

std::uint32_t Matched();

std::uint32_t Calls();
bool DrawLive();

const char* MapStatus();

const char* OcclusionNote();

const char* const* ShaderNames();
int ShaderNameCount();

std::string PreviewShaderSource();

}
}
}
