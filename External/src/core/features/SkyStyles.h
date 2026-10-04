#pragma once
#include "../globals/globals.h"
#include "../../render/menu/library.h"
#include <array>
namespace SkyStyles {
inline bool enabled=false,rotate=false;
inline int style=0;
inline float speed=3;
inline const char* names[]={"Pink clouds","Space","Cloudy night","Sparkling night","Minecraft"};
inline const char* ids[][6]={
 {"12635309703","12635311686","12635312870","12635313718","12635315817","12635316856"},
 {"12064107","12064152","12064121","12063984","12064115","12064131"},
 {"116758234","116758314","116758367","116758446","116758478","116758496"},
 {"1233158420","1233158838","1233157105","1233157640","1233157995","1233159158"},
 {"1876545003","1876544331","1876542941","1876543392","1876543764","1876544642"}
};
inline constexpr std::uintptr_t offsets[]={Offsets::Sky::SkyboxBk,Offsets::Sky::SkyboxDn,Offsets::Sky::SkyboxFt,Offsets::Sky::SkyboxLf,Offsets::Sky::SkyboxRt,Offsets::Sky::SkyboxUp};
inline std::uintptr_t savedSky=0;
inline std::array<std::string,6> original,applied;
inline RBX::Vec3 originalRotation{},lastRotation{};
inline int lastStyle=-1;
inline const char* status="Off";
inline bool ValidString(std::uintptr_t at){
    auto length=memory->Read<std::uint64_t>(at+16),capacity=memory->Read<std::uint64_t>(at+24);
    return length>0&&length<=capacity&&capacity<4096;
}
inline bool WriteExisting(std::uintptr_t at,const std::string& value){
    if(!ValidString(at)||value.size()>memory->Read<std::uint64_t>(at+24))return false;
    const auto dst=memory->Read<std::uint64_t>(at+24)>=16?memory->Read<std::uintptr_t>(at):at;
    if(dst<0x10000)return false;
    if(!memory->WriteRaw(dst,value.c_str(),value.size()+1))return false;
    return memory->Write<std::uint64_t>(at+16,value.size());
}
inline bool RefreshAvailable(){
    static std::uintptr_t checkedBase=0;static bool valid=false;
    const auto base=memory->get_module_address();if(!base)return false;
    if(checkedBase!=base){
        checkedBase=base;
        const unsigned char gate[]={0x80,0xb9,0x78,0x02,0,0,0,0x0f,0x85};
        const unsigned char finish[]={0x66,0xc7,0x86,0x78,0x02,0,0,0x01,0x01};
        const unsigned char textures[]={0x0f,0xb6,0x86,0xb5,0x04,0,0};
        unsigned char a[sizeof(gate)]{},b[sizeof(finish)]{},c[sizeof(textures)]{};
        valid=memory->ReadRaw(base+0x36ab792,a,sizeof(a))==sizeof(a)&&!std::memcmp(a,gate,sizeof(a))&&
              memory->ReadRaw(base+0x36ac4fa,b,sizeof(b))==sizeof(b)&&!std::memcmp(b,finish,sizeof(b))&&
              memory->ReadRaw(base+0x36ab7be,c,sizeof(c))==sizeof(c)&&!std::memcmp(c,textures,sizeof(c));
    }return valid;
}
inline bool Invalidate(bool textures=true){
    if(!RefreshAvailable())return false;
    const auto view=memory->Read<std::uintptr_t>(Globals::renderEngine.Addr+Offsets::VisualEngine::RenderView);
    const auto flags=memory->Read<std::uint16_t>(view+0x278);
    if(view<0x10000 || (flags&0xfefe))return false;

    if(textures){
        const auto loaded=memory->Read<std::uint8_t>(view+0x4b5);
        if(loaded>1||!memory->Write<std::uint8_t>(view+0x4b5,0))return false;
    }
    return memory->Write<std::uint16_t>(view+0x278,0);
}
inline void Restore(){
    if(savedSky && RBX::RbxInstance(savedSky).GetClass()=="Sky"){
        for(int i=0;i<6;++i)if(memory->read_string(savedSky+offsets[i])==applied[i])WriteExisting(savedSky+offsets[i],original[i]);
        auto rotation=memory->Read<RBX::Vec3>(savedSky+Offsets::Sky::SkyboxOrientation);
        if(!std::memcmp(&rotation,&lastRotation,sizeof(rotation)))memory->Write<RBX::Vec3>(savedSky+Offsets::Sky::SkyboxOrientation,originalRotation);
        Invalidate();
    }savedSky=0;lastStyle=-1;status="Off";
}
inline void Tick(){
    if(!enabled){if(savedSky)Restore();return;}
    if(!RefreshAvailable()){status="Sky refresh unavailable for this client build";return;}
    static auto next=std::chrono::steady_clock::time_point{};const auto tick=std::chrono::steady_clock::now();if(tick<next)return;next=tick+std::chrono::milliseconds(50);
    auto lighting=Globals::dataModel.FindChildByClass("Lighting");auto sky=lighting.FindChildByClass("Sky").Addr;
    if(!sky){status="This scene has no Sky instance";return;}
    if(savedSky!=sky){Restore();for(auto offset:offsets)if(!ValidString(sky+offset)){status="Sky layout could not be verified";return;}
        savedSky=sky;for(int i=0;i<6;++i)original[i]=memory->read_string(sky+offsets[i]);originalRotation=memory->Read<RBX::Vec3>(sky+Offsets::Sky::SkyboxOrientation);lastRotation=originalRotation;
    }
    style=std::clamp(style,0,4);
    if(lastStyle!=style){
        for(int i=0;i<6;++i){std::string value="rbxassetid://"+std::string(ids[style][i]);if(!WriteExisting(sky+offsets[i],value)){status="Sky texture storage is too small";return;}applied[i]=value;}
        if(Invalidate()){lastStyle=style;status="Sky refresh requested";}else status="Waiting for the sky renderer";
    }
    static auto previous=std::chrono::steady_clock::now();auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-previous).count();previous=now;
    if(rotate){lastRotation.Y=std::fmod(lastRotation.Y+(std::min)(dt,.1f)*speed,360.f);memory->Write<RBX::Vec3>(sky+Offsets::Sky::SkyboxOrientation,lastRotation);Invalidate(false);}
}
inline void Menu(){
    imGuiCustom::Checkbox("Sky textures",&enabled,ImVec2(12,51));
    imGuiCustom::Combo("sky_style",&style,names,5,ImVec2(12,101),272,"Sky:");
    imGuiCustom::Checkbox("Rotate sky",&rotate,ImVec2(12,151));
    imGuiCustom::SliderFloat("sky_speed",&speed,-30,30,ImVec2(12,210),272,"Rotation (degrees / second)","%.2f");
    ImGui::SetCursorPos(ImVec2(317,51));ImGui::PushTextWrapPos(690);ImGui::TextUnformatted(status);
    ImGui::SetCursorPos(ImVec2(317,115));ImGui::TextWrapped("Procedural animated sky needs the complete shader source. Rotation animates the selected texture sky.");ImGui::PopTextWrapPos();
}
}
