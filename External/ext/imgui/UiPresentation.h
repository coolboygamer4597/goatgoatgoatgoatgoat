#pragma once
#include "imgui.h"
#include <algorithm>
namespace UiPresentation {
inline float scale=1;
inline ImVec2 Logical(ImVec2 p){return ImVec2(p.x/scale,p.y/scale);}
inline ImVec2 Physical(ImVec2 p){return ImVec2(p.x*scale,p.y*scale);}
inline float Physical(float v){return v*scale;}
inline float Logical(float v){return v/scale;}
}
