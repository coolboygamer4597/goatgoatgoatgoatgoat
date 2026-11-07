#pragma once
#include "GameVisualsState.h"
#include "OwnedValue.h"
#include "../../../memory/memory.h"
#include "../../../sdk/RuntimeOffsets.h"
#include "../../../sdk/OffsetUpdates.h"
#include "../../globals/globals.h"
#include <array>
#include <map>
#include <vector>
#include <cstring>

namespace GameEnvironment {
struct Vector {float x=0,y=0,z=0;};
struct Layout {
    unsigned ambient=0,outdoor=0,brightness=0,exposure=0,fogColor=0,fogStart=0,fogEnd=0,latitude=0;
    unsigned clock=0,direction=0,sun=0,moon=0,source=0,light=0,gradientTop=0,gradientBottom=0;
    unsigned engine=0,lightingValid=0;
};
inline Layout layout;
inline Layout ResolveLayout(){
    if(Offsets::ClientVersion=="version-5a21b87bbc2641cf")
        return {0xB8,0xE8,0x100,0x10C,0xDC,0x118,0x114,0x11C,0xB0,0x150,0x160,0x16C,0x15C,0x144,0x138,0x178,0x18,0x278};
    const auto offset=[](const char* key){return static_cast<unsigned>(OffsetUpdates::OptionalField(key));};
    return {
        offset("Lighting.Ambient"),offset("Lighting.OutdoorAmbient"),offset("Lighting.Brightness"),
        offset("Lighting.ExposureCompensation"),offset("Lighting.FogColor"),offset("Lighting.FogStart"),
        offset("Lighting.FogEnd"),offset("Lighting.GeographicLatitude"),offset("Lighting.ClockTime"),
        offset("Lighting.LightDirection"),offset("Lighting.SunPosition"),offset("Lighting.MoonPosition"),
        offset("Lighting.Source"),offset("Lighting.LightColor"),offset("Lighting.GradientTop"),
        offset("Lighting.GradientBottom"),offset("RenderView.VisualEngine"),offset("RenderView.LightingValid")
    };
}
inline bool Complete(const Layout& offsets){
    return offsets.ambient&&offsets.outdoor&&offsets.brightness&&offsets.exposure&&offsets.fogColor&&
        offsets.fogStart&&offsets.fogEnd&&offsets.latitude&&offsets.clock&&offsets.direction&&offsets.sun&&
        offsets.moon&&offsets.source&&offsets.light&&offsets.gradientTop&&offsets.gradientBottom&&
        offsets.engine&&offsets.lightingValid&&Offsets::VisualEngine::RenderView;
}
struct Field {unsigned offset=0,size=0;GameVisuals::OwnedValue<std::array<unsigned char,16>> value;};
inline std::vector<Field> fields;
inline std::uintptr_t lighting=0,parent=0;
inline DWORD pid=0;
inline ULONGLONG next=0;
inline bool live(){return memory&&memory->IsConnected()&&memory->get_process_id()==pid&&lighting&&RBX::RbxInstance(lighting).GetClass()=="Lighting"&&RBX::RbxInstance(lighting).GetParent().Addr==parent;}
inline bool read(Field& f,std::array<unsigned char,16>& v){v={};return memory->ReadRaw(lighting+f.offset,v.data(),f.size)==f.size;}
inline bool write(Field& f,const std::array<unsigned char,16>& v){return memory->WriteRaw(lighting+f.offset,v.data(),f.size)==f.size;}
inline std::uintptr_t LightingFlag(){
    const auto engine=Globals::renderEngine.Addr;
    const auto view=engine?memory->Read<std::uintptr_t>(engine+Offsets::VisualEngine::RenderView):0;
    unsigned char valid=255;
    if(!view||memory->Read<std::uintptr_t>(view+layout.engine)!=engine||
        memory->ReadRaw(view+layout.lightingValid,&valid,1)!=1||valid>1)return 0;
    return view+layout.lightingValid;
}
inline bool Invalidate(){
    const auto flag=LightingFlag();
    const unsigned char off=0;
    return flag&&memory->WriteRaw(flag,&off,1)==1;
}
inline void Stop(){if(live()){for(auto& f:fields)f.value.Restore([&](auto& v){return read(f,v);},[&](const auto& v){return write(f,v);});Invalidate();}fields.clear();lighting=parent=0;pid=0;}
template<class T>inline bool Set(unsigned offset,bool enabled,const T& value){
    static_assert(sizeof(T)<=16);auto it=std::find_if(fields.begin(),fields.end(),[&](const Field& f){return f.offset==offset;});
    if(it==fields.end()){if(!enabled)return true;fields.push_back({offset,sizeof(T),{}});it=fields.end()-1;}
    if(it->size!=sizeof(T))return false;
    if(!enabled)return it->value.Restore([&](auto& v){return read(*it,v);},[&](const auto& v){return write(*it,v);});
    std::array<unsigned char,16> bytes{};std::memcpy(bytes.data(),&value,sizeof(value));return it->value.Apply(bytes,[&](auto& v){return read(*it,v);},[&](const auto& v){return write(*it,v);});
}
inline bool bounded(unsigned off,float low,float high){float v;return memory->ReadRaw(lighting+off,&v,4)==4&&std::isfinite(v)&&v>=low&&v<=high;}
inline bool vector(unsigned off,float low,float high){for(unsigned i=0;i<3;++i)if(!bounded(off+i*4,low,high))return false;return true;}
inline bool Valid(){
    std::int64_t clock=0;unsigned source=9;return bounded(layout.brightness,0,20)&&bounded(layout.exposure,-5,5)&&bounded(layout.fogEnd,0,1000000)&&bounded(layout.fogStart,0,1000000)&&bounded(layout.latitude,-180,180)&&vector(layout.ambient,0,1)&&vector(layout.outdoor,0,1)&&vector(layout.fogColor,0,1)&&vector(layout.gradientTop,0,4)&&vector(layout.light,0,4)&&vector(layout.gradientBottom,0,4)&&vector(layout.direction,-1.01f,1.01f)&&memory->ReadRaw(lighting+layout.clock,&clock,8)==8&&clock>=0&&clock<=86400000000ll&&memory->ReadRaw(lighting+layout.source,&source,4)==4&&source<=1;
}
inline void Tick(){
    if(GetTickCount64()<next)return;next=GetTickCount64()+50;
    auto& s=GameVisuals::settings;const bool active=s.ambient||s.brightness||s.exposure||s.fog||s.time;
    if(!active){if(lighting)Stop();GameVisuals::environmentStatus="Controls are off";return;}
    if(!memory||!memory->IsConnected()){Stop();GameVisuals::environmentStatus="Waiting for the game";return;}
    if(!lighting||pid!=memory->get_process_id()){
        Stop();
        layout=ResolveLayout();
    }
    if(!Complete(layout)){GameVisuals::environmentStatus="Lighting renderer offsets need verification for this client";return;}
    const auto current=Globals::dataModel.FindChildByClass("Lighting").Addr;
    if(pid!=memory->get_process_id()||current!=lighting||!live()){Stop();lighting=current;parent=Globals::dataModel.Addr;pid=memory->get_process_id();}
    if(!live()||!Valid()){Stop();GameVisuals::environmentStatus="Lighting values could not be verified";return;}
    if(!LightingFlag()){Stop();GameVisuals::environmentStatus="Lighting renderer could not be verified";return;}
    GameVisuals::Sanitize();bool ok=true;
    ok&=Set(layout.ambient,s.ambient,Vector{s.ambientColor[0],s.ambientColor[1],s.ambientColor[2]});ok&=Set(layout.outdoor,s.ambient,Vector{s.outdoorColor[0],s.outdoorColor[1],s.outdoorColor[2]});
    ok&=Set(layout.brightness,s.brightness,s.brightnessValue);ok&=Set(layout.exposure,s.exposure,s.exposureValue);
    ok&=Set(layout.fogColor,s.fog,Vector{s.fogColor[0],s.fogColor[1],s.fogColor[2]});ok&=Set(layout.fogStart,s.fog,s.fogStart);ok&=Set(layout.fogEnd,s.fog,s.fogEnd);
    const auto clock=static_cast<std::int64_t>(std::fmod(s.hour,24.f)*3600000000.0);
    const float latitude=memory->Read<float>(lighting+layout.latitude),angle=std::fmod(s.hour,24.f)*6.28318530718f/24.f,tilt=(latitude-23.5f)*.01745329252f;
    Vector sun{std::sin(angle)*std::cos(tilt),-std::cos(angle)*std::cos(tilt),std::sin(tilt)},moon{-sun.x,-sun.y,sun.z};
    const bool day=sun.y>-.15f;const Vector direction=day?sun:moon;const unsigned source=day?0:1;
    const float strength=std::clamp(sun.y*1.4f,0.f,1.f);const Vector light{.12f+.63f*strength,.14f+.61f*strength,.23f+.52f*strength},gradient{.2f+.8f*strength,.25f+.75f*strength,.4f+.6f*strength};
    ok&=Set(layout.clock,s.time,clock);ok&=Set(layout.direction,s.time,direction);ok&=Set(layout.sun,s.time,sun);ok&=Set(layout.moon,s.time,moon);ok&=Set(layout.source,s.time,source);ok&=Set(layout.light,s.time,light);ok&=Set(layout.gradientTop,s.time,gradient);ok&=Set(layout.gradientBottom,s.time,gradient);
    ok&=Invalidate();GameVisuals::environmentStatus=ok?"Active; original lighting restores when disabled":"Some lighting fields could not be changed";
}
}
