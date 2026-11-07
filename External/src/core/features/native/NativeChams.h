#pragma once

#include "NativeWorldDepth.h"
#include <cstdint>
#include <string>

namespace Cheat {
namespace Visuals {
namespace NativeChams {

void Prepare();
void ResetAimVisibility();
bool AimVisible(std::uint64_t identity,const float* point,const float* view,float* proven);
const char* StartupStatus();
bool Loaded();
const char* TextureTestStatus();
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
int ShaderMode(int index);
const char* const* WorldShaderNames();
int WorldShaderNameCount();

std::string PreviewShaderSource();

}
}
}
