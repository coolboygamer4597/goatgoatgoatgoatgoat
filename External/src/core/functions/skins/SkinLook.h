#pragma once
#include "../../variables/variables.h"
#include "../../app/CustomTextureData.h"
#include <array>
#include <cmath>
#include <sstream>
namespace SkinLook {
using Material=variables::Weapons::LimbMaterial;
struct Look {
    bool present=false,shading=true,cleanup=true,sniper=false,rifle=false,hide=false,preview=true;
    std::array<int,2> texture{{-1,-1}};
    std::array<bool,3> separate{};
    std::array<bool,variables::Weapons::customTextureCount> clean{};
    Material materials[2][3]{};
    Look(){clean.fill(true);}
};
inline Look Capture(){
    using namespace variables::Weapons;
    Look l;l.present=true;l.shading=textureShading;l.cleanup=modelCleanup;l.sniper=false;l.rifle=false;l.hide=hide;l.preview=preview;
    l.texture={texturePreset,glockTexturePreset};l.separate={separateWeapons,separateGloves,separateArms};
    for(int i=0;i<customTextureCount;++i)l.clean[i]=textureCleanSurface[i];
    for(int w=0;w<2;++w){l.materials[w][0]=MaterialForWeapon(w!=0);l.materials[w][1]=MaterialForLimb(w!=0,true);l.materials[w][2]=MaterialForLimb(w!=0,false);}
    return l;
}
inline bool ValidMaterial(const Material& m){
    if(m.style<0||m.style>=32)return false;
    auto range=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!range(m.opacity,0,1)||!range(m.speed,0,15)||!range(m.scale,.01f,100)||!range(m.glowStrength,0,10))return false;
    for(int i=0;i<4;++i)if(!range(m.color[i],0,1)||!range(m.glowColor[i],0,1))return false;
    return true;
}
inline bool Valid(const Look& l){
    if(!l.present)return true;
    for(int w=0;w<2;++w){if(l.texture[w]!=-1&&!CustomTextureData::Supports(l.texture[w],w!=0))return false;for(int g=0;g<3;++g)if(!ValidMaterial(l.materials[w][g]))return false;}
    return true;
}
inline void Apply(const Look& l){
    if(!l.present||!Valid(l))return;
    using namespace variables::Weapons;
    texturePreset=l.texture[0];glockTexturePreset=l.texture[1];textureShading=l.shading;modelCleanup=l.cleanup;hide=l.hide;preview=l.preview;
    textureCleanSurface.fill(true);
    SetSharedMaterial(l.materials[0][0]);arMaterial=l.materials[0][0];glockMaterial=l.materials[1][0];weaponMaterialsInitialized=true;separateWeapons=l.separate[0];
    gloves=arGloves=l.materials[0][1];glockGloves=l.materials[1][1];gloveMaterialsInitialized=true;separateGloves=l.separate[1];
    arms=arArms=l.materials[0][2];glockArms=l.materials[1][2];armMaterialsInitialized=true;separateArms=l.separate[2];
}
inline void Write(std::ostream& s,const Look& l){
    s<<"LOOK 2 "<<l.present<<' '<<l.shading<<' '<<l.cleanup<<' '<<l.sniper<<' '<<l.hide<<' '<<l.preview<<' '<<l.texture[0]<<' '<<l.texture[1]<<' '<<l.rifle<<'\n';
    for(bool v:l.separate)s<<v<<' ';s<<'\n';for(bool v:l.clean)s<<v<<' ';s<<'\n';
    for(int w=0;w<2;++w)for(const auto& m:l.materials[w]){s<<m.enabled<<' '<<m.glow<<' '<<m.style<<' '<<m.opacity<<' '<<m.speed<<' '<<m.scale<<' '<<m.glowStrength;for(float v:m.color)s<<' '<<v;for(float v:m.glowColor)s<<' '<<v;s<<'\n';}
}
inline bool Read(std::istream& s,Look& l){
    std::string tag;int version=0;
    if(!(s>>tag>>version)||tag!="LOOK"||(version!=1&&version!=2)||!(s>>l.present>>l.shading>>l.cleanup>>l.sniper>>l.hide>>l.preview>>l.texture[0]>>l.texture[1]))return false;
    if(version==2&&!(s>>l.rifle))return false;
    for(auto& v:l.separate)if(!(s>>v))return false;
    for(auto& v:l.clean)if(!(s>>v))return false;
    for(int w=0;w<2;++w)for(auto& m:l.materials[w]){if(!(s>>m.enabled>>m.glow>>m.style>>m.opacity>>m.speed>>m.scale>>m.glowStrength))return false;for(auto& v:m.color)if(!(s>>v))return false;for(auto& v:m.glowColor)if(!(s>>v))return false;}
    return Valid(l);
}
}
