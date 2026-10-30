#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
namespace WeaponTextureRuntime {
constexpr std::uint32_t createTexture=0xB00,createView=0xB08;
constexpr std::uint32_t bank=0xB10,bankStride=0x40,bankCount=4;
constexpr std::uint32_t texture=0,view=8,materialTexture=16,materialView=24,normalTexture=32,normalView=40,flags=48,hits=52;
constexpr std::uint32_t textureCommand=9,viewCommand=10,releaseCommand=11,releaseBundleCommand=14;
constexpr std::uint32_t targetFlag=0x400,glockFlag=0x800,groupShift=12,groupMask=3<<groupShift;
constexpr std::uint32_t colorSlot=3,nativeOverlayFlag=0x8000;
constexpr std::uint32_t projectionFlag=0x20000;
constexpr std::uint32_t Field(int group,int channel,bool srv){return bank+group*bankStride+channel*16+(srv?8:0);}
static_assert(bank+bankStride*bankCount<=0x1000);
inline bool MatchesX5(std::string_view mesh) {
    constexpr std::string_view prefix="rbxassetid://";
    if(mesh.substr(0,prefix.size())==prefix)mesh.remove_prefix(prefix.size());
    else if(auto at=mesh.find("id=");at!=mesh.npos)mesh.remove_prefix(at+3);
    const auto end=mesh.find_first_not_of("0123456789");
    if(end!=mesh.npos)mesh=mesh.substr(0,end);
    for(auto id:{"105137249332183","81766375295784","93054331126203","95891610475658",
        "118641859394860","71700100568681","108003260543980","86598522525666",
        "110878660637855","74915530422476","86071090387346","77729921739121"})
        if(mesh==id)return true;
    return false;
}
struct Code {
    std::vector<std::uint8_t>& data;
    void Bytes(std::initializer_list<std::uint8_t> bytes){data.insert(data.end(),bytes);}
    void D32(std::uint32_t value){for(int i=0;i<4;++i)data.push_back(std::uint8_t(value>>(i*8)));}
    void U64(std::uint64_t value){for(int i=0;i<8;++i)data.push_back(std::uint8_t(value>>(i*8)));}
    std::size_t Jump(std::uint8_t condition=0xFF){if(condition==0xFF)Bytes({0xE9});else Bytes({0x0F,std::uint8_t(0x80|condition)});D32(0);return data.size()-4;}
    void Patch(std::size_t at){std::int32_t relative=static_cast<std::int32_t>(data.size()-at-4);std::memcpy(data.data()+at,&relative,4);}
    void State(std::uint64_t state){Bytes({0x48,0xB8});U64(state);}
    void Call(std::uint32_t field){Bytes({0xFF,0x90});D32(field);}
};
inline void AppendCommands(std::vector<std::uint8_t>& bytes,std::uint64_t state) {
    Code c{bytes};c.State(state);c.Bytes({0x8B,0x48,4,0x83,0xF9,textureCommand});auto textureCase=c.Jump(4);
    c.Bytes({0x83,0xF9,viewCommand});auto viewCase=c.Jump(4);
    c.Bytes({0x83,0xF9,releaseCommand});auto releaseCase=c.Jump(4);
    c.Bytes({0x83,0xF9,releaseBundleCommand});auto releaseBundleCase=c.Jump(4);auto unrelated=c.Jump();
    c.Patch(textureCase);c.Bytes({0x48,0x83,0xEC,0x48});
    c.Bytes({0x48,0xC7,0x80});c.D32(0x90);c.D32(0);
    c.Bytes({0x48,0x8B,0x48,0x10,0x48,0x8D,0x90});c.D32(0x200);
    c.Bytes({0x4C,0x8B,0x80});c.D32(0x98);c.Bytes({0x4C,0x8D,0x88});c.D32(0x90);c.Call(createTexture);auto textureDone=c.Jump();
    c.Patch(viewCase);c.Bytes({0x48,0x83,0xEC,0x48});
    c.Bytes({0x48,0xC7,0x80});c.D32(0x90);c.D32(0);
    c.Bytes({0x48,0x8B,0x48,0x10,0x48,0x8B,0x90});c.D32(0x80);
    c.Bytes({0x45,0x31,0xC0,0x4C,0x8D,0x88});c.D32(0x90);c.Call(createView);auto viewDone=c.Jump();
    auto releaseSetup=[&](std::size_t branch,bool bundle){
        c.Patch(branch);c.Bytes({0x53,0x56,0x57,0x48,0x83,0xEC,0x20});
        c.State(state);
        if(bundle){c.Bytes({0x48,0x8B,0x98});c.D32(0x80);}
        else{c.Bytes({0x48,0x8D,0x98});c.D32(bank);}
        c.Bytes({0xBE,4,0,0,0,0xBF});c.D32(bundle?48:bankStride);
    };
    releaseSetup(releaseCase,false);const auto releaseLoop=c.Jump();releaseSetup(releaseBundleCase,true);c.Patch(releaseLoop);
    const auto loop=bytes.size();c.Bytes({0x83,0xFF,bankStride});const auto packed=c.Jump(5);
    c.Bytes({0xC7,0x43,flags,0,0,0,0});c.Patch(packed);
    for(auto field:{view,texture,materialView,materialTexture,normalView,normalTexture}){
        c.Bytes({0x48,0x8B,0x4B,std::uint8_t(field),0x48,0x85,0xC9});const auto empty=c.Jump(4);
        c.Bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});c.Patch(empty);
        c.Bytes({0x48,0xC7,0x43,std::uint8_t(field),0,0,0,0});
    }
    c.Bytes({0x48,0x01,0xFB,0xFF,0xCE});const auto again=c.Jump(5);const std::int32_t back=static_cast<std::int32_t>(loop)-static_cast<std::int32_t>(again+4);std::memcpy(bytes.data()+again,&back,4);
    c.State(state);c.Bytes({0x48,0xC7,0x80});c.D32(0x90);c.D32(1);
    c.Bytes({0xC7,0x40,4,0,0,0,0,0xFF,0x80});c.D32(0xB0);c.Bytes({0xC7,0x80});c.D32(0x1F0);c.D32(0);
    c.Bytes({0x48,0x83,0xC4,0x20,0x5F,0x5E,0x5B,0xC3});
    c.Patch(textureDone);c.Patch(viewDone);c.Bytes({0x48,0xB9});c.U64(state);c.Bytes({0x89,0x81});c.D32(0x1F0);
    c.Bytes({0xC7,0x41,4,0,0,0,0,0xFF,0x81});c.D32(0xB0);
    c.Bytes({0x48,0x83,0xC4,0x48,0xC3});c.Patch(unrelated);
}
template<class OriginalDraw,class ProjectedDraw,class NativeFallback>
std::vector<std::size_t> AppendDraw(std::vector<std::uint8_t>& bytes,std::uint32_t weaponOffset,OriginalDraw&& draw,ProjectedDraw&& projectedDraw,NativeFallback&& nativeFallback){
    Code c{bytes};std::vector<std::size_t> done,fallback;
    auto finish=[&]{c.Bytes({0x49,0xC7,0x46,0x70,0,0,0,0});done.push_back(c.Jump());};
    c.Bytes({0x48,0x8B,0x44,0x24,0x58,0xF7,0x80});c.D32(weaponOffset);c.D32(targetFlag);auto unrelated=c.Jump(4);
    constexpr unsigned strideShift=6;
    static_assert(bankStride==(1u<<strideShift)&&groupShift>=strideShift);
    c.Bytes({0x8B,0x80});c.D32(weaponOffset);c.Bytes({0x25});c.D32(groupMask);c.Bytes({0xC1,0xE8,groupShift-strideShift,0x89,0x84,0x24});c.D32(0x10C);
    auto offset=[&]{c.Bytes({0x8B,0x84,0x24});c.D32(0x10C);};
    c.Bytes({0x41,0xF7,0x84,0x06});c.D32(bank+flags);c.D32(1);fallback.push_back(c.Jump(4));
    c.Bytes({0x49,0x83,0xBC,0x06});c.D32(bank+view);c.Bytes({0});fallback.push_back(c.Jump(4));
    c.Bytes({0xC7,0x84,0x24});c.D32(0x108);c.D32(0);
    auto bind=[&](std::uint32_t slot,std::uint32_t field,std::uint32_t saved){
        c.Bytes({0x49,0x8B,0x4E,0x18,0xBA});c.D32(slot);c.Bytes({0x41,0xB8,1,0,0,0,0x4C,0x8D,0x8C,0x24});c.D32(saved);
        c.Bytes({0x49,0x8B,0x86});c.D32(0x398);c.Bytes({0xFF,0xD0});
        offset();c.Bytes({0x49,0x8B,0x4E,0x18,0xBA});c.D32(slot);c.Bytes({0x41,0xB8,1,0,0,0,0x4D,0x8D,0x8C,0x06});c.D32(bank+field);
        c.Bytes({0x49,0x8B,0x86});c.D32(0x3A0);c.Bytes({0xFF,0xD0});
    };
    bind(3,view,0x120);
    for(auto channel:{1,2}){
        const auto bit=1u<<channel,field=channel==1?materialView:normalView,saved=channel==1?0x118u:0x110u;
        offset();c.Bytes({0x41,0xF7,0x84,0x06});c.D32(bank+flags);c.D32(bit);auto disabled=c.Jump(4);
        c.Bytes({0x49,0x83,0xBC,0x06});c.D32(bank+field);c.Bytes({0});auto missing=c.Jump(4);
        bind(channel==1?5:4,field,saved);
        c.Bytes({0x81,0x8C,0x24});c.D32(0x108);c.D32(bit);c.Patch(disabled);c.Patch(missing);
    }
    if constexpr(std::is_same_v<std::remove_reference_t<OriginalDraw>,std::remove_reference_t<ProjectedDraw>>)draw();
    else{c.Bytes({0x48,0x8B,0x44,0x24,0x58,0xF7,0x80});c.D32(weaponOffset);c.D32(projectionFlag);const auto standard=c.Jump(4);projectedDraw();const auto drawn=c.Jump();c.Patch(standard);draw();c.Patch(drawn);}
    offset();c.Bytes({0x41,0xFF,0x84,0x06});c.D32(bank+hits);
    auto restore=[&](std::uint32_t slot,std::uint32_t saved){
        c.Bytes({0x49,0x8B,0x4E,0x18,0xBA});c.D32(slot);c.Bytes({0x41,0xB8,1,0,0,0,0x4C,0x8D,0x8C,0x24});c.D32(saved);
        c.Bytes({0x49,0x8B,0x86});c.D32(0x3A0);c.Bytes({0xFF,0xD0});
        c.Bytes({0x48,0x8B,0x8C,0x24});c.D32(saved);c.Bytes({0x48,0x85,0xC9});auto empty=c.Jump(4);
        c.Bytes({0x48,0x8B,0x01,0xFF,0x50,0x10});c.Patch(empty);
    };
    for(auto channel:{1,2}){
        c.Bytes({0xF7,0x84,0x24});c.D32(0x108);c.D32(1u<<channel);auto untouched=c.Jump(4);
        restore(channel==1?5:4,channel==1?0x118:0x110);c.Patch(untouched);
    }
    restore(3,0x120);
    c.Bytes({0x48,0x8B,0x44,0x24,0x58,0xF7,0x80});c.D32(weaponOffset);c.D32(nativeOverlayFlag);const auto overlay=c.Jump(5);finish();
    for(auto at:fallback)c.Patch(at);nativeFallback();finish();c.Patch(unrelated);c.Patch(overlay);return done;
}
template<class OriginalDraw,class ProjectedDraw>
std::vector<std::size_t> AppendDraw(std::vector<std::uint8_t>& bytes,std::uint32_t weaponOffset,OriginalDraw&& draw,ProjectedDraw&& projectedDraw){return AppendDraw(bytes,weaponOffset,draw,projectedDraw,draw);}
template<class OriginalDraw>
std::vector<std::size_t> AppendDraw(std::vector<std::uint8_t>& bytes,std::uint32_t weaponOffset,OriginalDraw&& draw){return AppendDraw(bytes,weaponOffset,draw,draw);}
}
