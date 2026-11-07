#pragma once
#include <chrono>
#include "UiAssets.h"
namespace MenuMotion {
inline float alpha=0;
inline bool previous=false;
inline void Update(bool open,bool animate){
    static auto last=std::chrono::steady_clock::now();auto now=std::chrono::steady_clock::now();
    float dt=std::chrono::duration<float>(now-last).count();last=now;
    const bool changed=previous!=open;
    if(changed){
        UiAssets::Sound(true);
        previous=open;
    }
    if(!animate){alpha=open?1.f:0.f;return;}
    if(changed)return;
    const float step=(std::min)(dt,1.f/60.f)*3.2f;
    alpha=open?(std::min)(1.f,alpha+step):(std::max)(0.f,alpha-step);
}
inline bool Visible(){return alpha>0.001f;}
}
