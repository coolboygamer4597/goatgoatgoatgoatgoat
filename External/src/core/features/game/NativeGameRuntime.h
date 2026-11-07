#pragma once
#include "../native/WeaponTextureRuntime.h"
namespace NativeGameRuntime {
inline constexpr unsigned initialize=0xC40,release=0xC48,sky=0xC50,post=0xC58,request=0xC60,resolve=0xC68,targets=0xC70,originalResolve=0xC78,originalTargets=0xC80,depth=0xC88,before=0xC90,createCommand=16,releaseCommand=17;
inline constexpr std::uint64_t worldFlag=0x200000;
static_assert(originalTargets+8<=0x1000);
inline void AppendCommands(std::vector<std::uint8_t>& bytes,std::uint64_t state){
    WeaponTextureRuntime::Code c{bytes};c.State(state);c.Bytes({0x83,0x78,4,createCommand});const auto create=c.Jump(4);c.Bytes({0x83,0x78,4,releaseCommand});const auto destroy=c.Jump(4);const auto unrelated=c.Jump();
    for(auto entry:{std::pair<std::size_t,unsigned>{create,initialize},{destroy,release}}){
        c.Patch(entry.first);c.Bytes({0xBA,1,0,0,0,0x48,0x87,0x50,0x70,0x48,0x85,0xD2});const auto acquired=c.Jump(4);c.Bytes({0xC3});c.Patch(acquired);
        c.Bytes({0x48,0x83,0xEC,0x28,0x48,0x8B,0x88});c.D32(request);if(entry.second==release){c.Bytes({0x48,0xC7,0x80});c.D32(request);c.D32(0);}
        c.Bytes({0x48,0x8B,0x80});c.D32(entry.second);c.Bytes({0xFF,0xD0});if(entry.second==initialize)c.Bytes({0x0F,0xB6,0xC0});else c.Bytes({0xB8,1,0,0,0});
        c.Bytes({0x48,0xB9});c.U64(state);c.Bytes({0x48,0x89,0x81});c.D32(0x90);c.Bytes({0x48,0xC7,0x41,0x70,0,0,0,0,0xC7,0x41,4,0,0,0,0,0xFF,0x81});c.D32(0xB0);c.Bytes({0x48,0x83,0xC4,0x28,0xC3});
    }
    c.Patch(unrelated);
}
}
