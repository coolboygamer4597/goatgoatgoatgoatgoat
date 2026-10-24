#pragma once
#include "WeaponTextureRuntime.h"
namespace NativeReplacementRuntime {
inline constexpr unsigned initialize=0xC18,draw=0xC20,release=0xC28,request=0xC30;
inline constexpr unsigned createCommand=12,releaseCommand=13,modelFlag=0x40000,primaryFlag=0x80000;
inline void AppendCommands(std::vector<std::uint8_t>& bytes,std::uint64_t state){
    WeaponTextureRuntime::Code c{bytes};c.State(state);c.Bytes({0x83,0x78,4,createCommand});const auto create=c.Jump(4);c.Bytes({0x83,0x78,4,releaseCommand});const auto destroy=c.Jump(4);const auto unrelated=c.Jump();
    for(auto entry:{std::pair<std::size_t,unsigned>{create,initialize},{destroy,release}}){c.Patch(entry.first);c.Bytes({0x48,0x83,0xEC,0x28,0x48,0x8B,0x88});c.D32(request);c.Bytes({0x48,0x8B,0x80});c.D32(entry.second);c.Bytes({0xFF,0xD0});if(entry.second==initialize)c.Bytes({0x0F,0xB6,0xC0});else c.Bytes({0xB8,1,0,0,0});c.Bytes({0x48,0xB9});c.U64(state);c.Bytes({0x48,0x89,0x81});c.D32(0x90);c.Bytes({0xC7,0x41,4,0,0,0,0,0xFF,0x81});c.D32(0xB0);c.Bytes({0x48,0x83,0xC4,0x28,0xC3});}
    c.Patch(unrelated);
}
}
