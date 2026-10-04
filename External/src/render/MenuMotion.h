#pragma once
#include <chrono>
#include "UiAssets.h"
namespace MenuMotion {
inline float alpha=0;
inline bool previous=false;
inline void Update(bool open){
    static auto last=std::chrono::steady_clock::now();auto now=std::chrono::steady_clock::now();
    float dt=std::chrono::duration<float>(now-last).count();last=now;
    if(previous!=open){UiAssets::Sound(true);previous=open;}
    const float step=(std::min)(dt,.05f)*5;
    alpha=open?(std::min)(1.f,alpha+step):(std::max)(0.f,alpha-step);
}
inline bool Visible(){return alpha>0.001f;}
}
