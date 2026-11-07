#pragma once
#include "GameVisualsState.h"
#include "../../variables/variables.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
namespace StretchResolution {
inline std::string status="Off";
inline float Factor(float width,float height){if(!variables::Misc::stretchResolution||width<64||height<64)return 1;const float ratios[]={4.f/3,5.f/4,16.f/10};return std::clamp((width/height)/ratios[std::clamp(variables::Misc::stretchAspect,0,2)],1.f,3.f);}
inline void Tick(){RECT rect{};const auto game=FindWindowW(nullptr,L"Roblox");const float factor=game&&GetClientRect(game,&rect)?Factor(float(rect.right),float(rect.bottom)):1;GameVisuals::settings.stretchFactor=factor;status=!variables::Misc::stretchResolution?"Off":factor>1.0001f?"Active":"Aspect ratio is wider than this window";}
inline float AppliedFactor(){return variables::Misc::stretchResolution?GameVisuals::appliedStretch:1.f;}
inline void Stop(){GameVisuals::settings.stretchFactor=GameVisuals::appliedStretch=1;}
}
