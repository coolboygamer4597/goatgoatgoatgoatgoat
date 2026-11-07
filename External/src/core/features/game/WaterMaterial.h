#pragma once
#include "GameVisualsState.h"
#include "WaterTrail.h"
#include "../native/NativePattern.h"
#include <cstring>
namespace WaterMaterial {
inline int Mode(int style){constexpr int modes[]={39,40,33,34,36,37,38};return modes[std::clamp(style,0,6)];}
inline bool UsesWater(int style){return style==32||style==33||style==35||style==36||style==38||style==39;}
inline void Apply(NativePattern::Material& m,const GameVisuals::Settings& s,const WaterTrail::Trail& trail,double now,float phase){
    m.org_pos[0]=s.water?1.f:0.f;m.org_pos[1]=static_cast<float>(Mode(s.waterStyle));m.org_pos[2]=s.world?1.f:0.f;m.org_pos[3]=s.waterOpacity*(s.waterOriginal?.45f:1.f);
    m.org_x[0]=s.waterWaveScale;m.org_x[1]=s.waterWaveHeight;m.org_x[2]=s.waterRipples?s.waterRippleStrength:0;m.org_x[3]=s.waterRippleLifetime;
    std::memcpy(m.org_y,s.waterColor,16);m.org_z[0]=s.waterRippleSize;m.org_z[1]=s.waterRoughness;m.org_z[2]=s.waterReflection;m.org_z[3]=s.waterLevel;
    m.world_depth.camera_range[0]=phase;m.world_depth.info[0]=1;
    for(unsigned i=0;i<6;++i){auto sample=trail.Sample(i,now,s.waterRippleLifetime);float* dst=i<3?m.projection_rows+i*4:i==3?m.projection_minimum:i==4?m.projection_inverse_span:m.projection_options;std::memcpy(dst,sample.data(),16);}
}
}
