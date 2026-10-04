#pragma once
#include "../cache/cache.h"
#include "../app/app.h"
#include "../../render/menu/library.h"
namespace TargetLabels {
inline bool enabled[3]={false,false,false};
inline float colors[3][4]={{1,1,1,1},{.65f,.85f,1,1},{.4f,1,.6f,1}};
inline float sizes[3]={16,16,16};
inline int positions[3]={0,1,0};
inline std::unordered_map<std::uint64_t,bool> friends;
inline bool IsFriend(std::uint64_t id){
    const auto local=memory->Read<std::uint64_t>(Globals::localPlayer.Addr+Offsets::Player::UserId);
    if(!local||!id||local==id)return false;
    const auto service=Globals::dataModel.FindChildByClass("FriendService").Addr;if(!service)return false;
    const auto mask=memory->Read<std::uint64_t>(service+0x2A0);
    const auto buckets=memory->Read<std::uintptr_t>(service+0x288),sentinel=memory->Read<std::uintptr_t>(service+0x278);
    if(!mask||mask>1048575||((mask+1)&mask)||buckets<0x10000)return false;
    std::uint64_t hash=local+0x9E3779B9ULL;hash^=id+(hash<<6)+(hash>>2)+0x9E3779B9ULL;
    const auto bucket=buckets+(hash&mask)*16;
    auto current=memory->Read<std::uintptr_t>(bucket+8);const auto end=memory->Read<std::uintptr_t>(bucket);
    struct Node{std::uint64_t prev,next,user,other;std::uint32_t status;};
    std::unordered_set<std::uint64_t> seen;
    for(int i=0;i<128 && current>=0x10000 && current!=sentinel && seen.insert(current).second;++i){
        Node n{};if(memory->ReadRaw(current,&n,sizeof(n))!=sizeof(n))return false;
        if(n.user==local&&n.other==id)return n.status==2;
        if(current==end)break;current=n.next;
    }return false;
}
inline bool Active(){return enabled[0]||enabled[1]||enabled[2];}
inline void Draw(ImDrawList* dl){
    if(!Active())return;
    static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();
    if(enabled[2]&&now>=next){next=now+std::chrono::seconds(1);friends.clear();for(auto& p:PlayerCache::players)friends[p.userId]=IsFriend(p.userId);}
    auto vm=Globals::renderEngine.GetViewMat();auto size=ImGui::GetIO().DisplaySize;
    auto project=[&](RBX::Vec3 p,ImVec2& out){auto& m=vm.data;float w=p.X*m[12]+p.Y*m[13]+p.Z*m[14]+m[15];if(w<.01f)return false;out=ImVec2((1+(p.X*m[0]+p.Y*m[1]+p.Z*m[2]+m[3])/w)*size.x*.5f,(1-(p.X*m[4]+p.Y*m[5]+p.Z*m[6]+m[7])/w)*size.y*.5f);return true;};
    auto font=imGuiCustom::GetFonts().CascadiaMonoBL;
    for(auto& p:PlayerCache::players){
        if(p.playerAddr==Globals::localPlayer.Addr||PlayerRules::Esp(p.userId)||!App::PassesChamChecks(p.isValid,p.characterAddr,p.teamAddr,p.health))continue;
        auto root=RBX::RbxInstance(p.rootPartAddr).GetPos();ImVec2 top,bottom;
        if(!project({root.X,root.Y+3,root.Z},top)||!project({root.X,root.Y-3,root.Z},bottom))continue;
        float stacks[4]{};
        for(int i=0;i<3;++i){if(!enabled[i]||(i==2&&!friends[p.userId]))continue;
            std::string text=i==0?p.name:i==1?std::to_string(int(p.distance))+" studs":"Friend";
            float sz=std::clamp(sizes[i],8.f,48.f);auto extent=font->CalcTextSizeA(sz,FLT_MAX,0,text.c_str());int side=std::clamp(positions[i],0,3);ImVec2 at;
            float half=std::abs(bottom.y-top.y)*.25f;
            if(side==0)at=ImVec2(top.x-extent.x*.5f,top.y-extent.y-4-stacks[side]);
            else if(side==1)at=ImVec2(bottom.x-extent.x*.5f,bottom.y+4+stacks[side]);
            else at=ImVec2(top.x+(side==2?-half-extent.x-5:half+5),(top.y+bottom.y)*.5f+stacks[side]);
            stacks[side]+=sz+2;dl->AddText(font,sz,at+ImVec2(1,1),IM_COL32(0,0,0,230),text.c_str());dl->AddText(font,sz,at,ImGui::ColorConvertFloat4ToU32(*reinterpret_cast<ImVec4*>(colors[i])),text.c_str());
        }
    }
}
inline void Menu(){
    const char* names[]={"Name","Distance","Friend marker"};const char* sides[]={"Above","Below","Left","Right"};float y=51;
    for(int i=0;i<3;++i){ImGui::PushID(i);imGuiCustom::Checkbox(names[i],&enabled[i],ImVec2(317,y));imGuiCustom::ColorSquare("label_color",reinterpret_cast<ImVec4*>(colors[i]),ImVec2(670,y));y+=25;
        if(enabled[i]){imGuiCustom::SliderFloat("label_size",&sizes[i],8,48,ImVec2(317,y+imGuiCustom::SliderTop()),360,"Text Size","%.0f");y+=imGuiCustom::SliderStep();imGuiCustom::Combo("label_side",&positions[i],sides,4,ImVec2(317,y+imGuiCustom::ComboTop()),260,"Placement:");y+=imGuiCustom::ComboStep()+imGuiCustom::ComboTop()+12;}ImGui::PopID();}
}
}
