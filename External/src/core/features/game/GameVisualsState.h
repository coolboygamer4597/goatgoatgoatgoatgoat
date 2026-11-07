#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace GameVisuals {
struct Settings {
    float stretchFactor=1;
    bool sky=false,skyAnimate=true,skyTint=false;
    int skyStyle=0;
    float skySpeed=.25f,skyIntensity=1,skyScale=1,skyRotation=0;
    float skyPrimary[3]={.32f,.48f,1.f},skySecondary[3]={.92f,.24f,.65f};
    bool world=false,worldOriginal=false,worldGlow=false;
    int worldStyle=1,worldKinds=0,materialLayout=2;
    float worldColor[4]={.25f,.65f,.9f,1.f},worldOpacity=1,worldSpeed=1,worldScale=1;
    float worldGlowColor[4]={.2f,.65f,1.f,1.f},worldGlowStrength=.25f,worldDistance=2500;
    static constexpr bool water=false;
    bool waterRipples=true,waterOriginal=true;int waterStyle=0;
    float waterColor[4]={.035f,.22f,.29f,1},waterOpacity=.9f,waterWaveScale=8,waterWaveHeight=.12f,waterWaveSpeed=1;
    float waterRoughness=.10f,waterReflection=.9f,waterLevel=0;
    float waterRippleStrength=1,waterRippleLifetime=2,waterRippleSize=1,waterTrailSpacing=1.6f;
    bool weather=false,worldReflection=false,reflectionWalls=false;int weatherStyle=0;float weatherDensity=.5f,weatherSpeed=1,weatherColor[4]={.65f,.78f,1.f,.35f},reflectionStrength=.55f,reflectionRoughness=.15f,reflectionDistance=80;
    static constexpr bool fog=false;
    bool ambient=false,brightness=false,exposure=false,time=false,glow=false;
    float ambientColor[3]={.5f,.5f,.5f},outdoorColor[3]={.5f,.5f,.5f};
    float brightnessValue=2,exposureValue=0,fogColor[3]={.6f,.65f,.75f},fogStart=100,fogEnd=1500;
    float hour=14,glowIntensity=.25f,glowRadius=8,glowThreshold=.6f;
};
inline Settings settings;
inline float appliedStretch=1;
inline std::string environmentStatus="Controls are off",worldStatus="Off",skyStatus="Off";
inline bool NativeNeeded(){return settings.stretchFactor>1.0001f||settings.world||settings.water||settings.sky||settings.glow||settings.fog||settings.weather||settings.worldReflection;}
inline int WorldMaterialMode(int style){return style==32?35:style;}
inline void MigrateMaterials(){
    auto& s=settings;if(s.worldStyle<32)return;const int old=s.worldStyle;
    if(old==34){s.worldStyle=32;return;}
    if(old<=39)s.world=false;
    s.worldStyle=1;
}
inline void Sanitize(){
    auto& s=settings;
    s.skyStyle=std::clamp(s.skyStyle,0,9);s.worldStyle=std::clamp(s.worldStyle,0,32);s.worldKinds=std::clamp(s.worldKinds,0,2);
    s.waterLevel=std::clamp(s.waterLevel,-12.f,0.f);s.waterRoughness=std::clamp(s.waterRoughness,0.f,1.f);s.waterReflection=std::clamp(s.waterReflection,0.f,2.f);s.waterStyle=std::clamp(s.waterStyle,0,6);s.waterOpacity=std::clamp(s.waterOpacity,0.f,1.f);s.waterWaveScale=std::clamp(s.waterWaveScale,1.f,40.f);s.waterWaveHeight=std::clamp(s.waterWaveHeight,0.f,.5f);s.waterWaveSpeed=std::clamp(s.waterWaveSpeed,0.f,3.f);
    s.waterRippleStrength=std::clamp(s.waterRippleStrength,0.f,2.f);s.waterRippleLifetime=std::clamp(s.waterRippleLifetime,.4f,4.f);s.waterRippleSize=std::clamp(s.waterRippleSize,.25f,3.f);s.waterTrailSpacing=std::clamp(s.waterTrailSpacing,.6f,4.f);
    s.skySpeed=std::clamp(s.skySpeed,0.f,3.f);s.skyIntensity=std::clamp(s.skyIntensity,0.f,3.f);s.skyScale=std::clamp(s.skyScale,.4f,2.5f);s.skyRotation=std::clamp(s.skyRotation,-180.f,180.f);
    s.worldOpacity=std::clamp(s.worldOpacity,0.f,1.f);s.worldSpeed=std::clamp(s.worldSpeed,0.f,15.f);s.worldScale=std::clamp(s.worldScale,.25f,8.f);s.worldGlowStrength=std::clamp(s.worldGlowStrength,0.f,2.f);s.worldDistance=std::clamp(s.worldDistance,50.f,10000.f);
    s.weatherStyle=std::clamp(s.weatherStyle,0,2);s.weatherDensity=std::clamp(s.weatherDensity,0.f,1.f);s.weatherSpeed=std::clamp(s.weatherSpeed,0.f,3.f);s.reflectionStrength=std::clamp(s.reflectionStrength,0.f,1.f);s.reflectionRoughness=std::clamp(s.reflectionRoughness,0.f,1.f);s.reflectionDistance=std::clamp(s.reflectionDistance,10.f,200.f);
    s.brightnessValue=std::clamp(s.brightnessValue,0.f,8.f);s.exposureValue=std::clamp(s.exposureValue,-3.f,3.f);s.hour=std::clamp(s.hour,0.f,24.f);s.fogStart=std::clamp(s.fogStart,0.f,99999.f);s.fogEnd=std::clamp(s.fogEnd,s.fogStart+1.f,100000.f);
    if(s.glowThreshold>1.f)s.glowThreshold=.6f;
    s.glowIntensity=std::clamp(s.glowIntensity,0.f,2.f);s.glowRadius=std::clamp(s.glowRadius,1.f,24.f);s.glowThreshold=std::clamp(s.glowThreshold,0.f,.95f);
    for(auto* c:{s.skyPrimary,s.skySecondary,s.ambientColor,s.outdoorColor,s.fogColor,s.worldColor,s.worldGlowColor,s.waterColor})for(int i=0;i<3;++i)c[i]=std::clamp(c[i],0.f,1.f);
}
}
