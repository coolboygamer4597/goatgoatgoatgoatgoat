#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace NativeOcclusion {

constexpr std::uint64_t referenceRva = 0x3A28D8B;
constexpr std::uint64_t recordRva = 0x862BC90;
constexpr std::uint32_t clientTimestamp = 0x081E2DA2;
constexpr std::uint32_t clientImageSize = 0x0909E000;

template<class Read>
std::uint64_t resolve(std::uint64_t base, std::uint64_t size, Read read) {
    if(!base || size > ~std::uint64_t(0)-base)return 0;
    auto inside=[&](std::uint64_t p,std::uint64_t n){return p>=base && n<=size && p-base<=size-n;};
    auto namedFlag=[&](std::uint64_t flag)->std::uint64_t {
        struct Entry {std::uint64_t value,name,length;} entry{};
        if(!inside(flag,sizeof(entry)) || !read(flag,&entry,sizeof(entry)) || entry.value>1)return 0;

        constexpr char expected[]="RenderFastClusterOcclusionCulling";
        char name[sizeof(expected)]{};
        if(entry.length!=sizeof(expected)-1 || !inside(entry.name,sizeof(name)) ||
           !read(entry.name,name,sizeof(name)) || std::memcmp(name,expected,sizeof(name))!=0)return 0;
        return flag;
    };
    unsigned char code[18]{};
    const auto at=base+referenceRva;
    if(inside(at,sizeof(code)) && read(at,code,sizeof(code))) {

        const unsigned char tail[]={0,0x4d,0x8b,0xf1,0x4c,0x8b,0xbc,0x24,0xb8,0x18,0,0};
        if(code[0]!=0x80 || code[1]!=0x3d || std::memcmp(code+6,tail,sizeof(tail))!=0)return 0;
        std::int32_t displacement=0;std::memcpy(&displacement,code+2,4);
        const auto flag=static_cast<std::uint64_t>(static_cast<std::int64_t>(at+7)+displacement);
        return namedFlag(flag);
    }

    if(size!=clientImageSize)return 0;
    unsigned char dos[64]{};
    if(!inside(base,sizeof(dos)) || !read(base,dos,sizeof(dos)) || dos[0]!='M' || dos[1]!='Z')return 0;
    std::uint32_t ntRva=0;std::memcpy(&ntRva,dos+60,4);
    unsigned char nt[84]{};
    const auto pe=base+ntRva;
    if(!inside(pe,sizeof(nt)) || !read(pe,nt,sizeof(nt)))return 0;
    std::uint32_t signature=0,timestamp=0,imageSize=0;
    std::uint16_t machine=0,optionalSize=0,magic=0;
    std::memcpy(&signature,nt,4);std::memcpy(&machine,nt+4,2);
    std::memcpy(&timestamp,nt+8,4);std::memcpy(&optionalSize,nt+20,2);
    std::memcpy(&magic,nt+24,2);std::memcpy(&imageSize,nt+80,4);
    if(signature!=0x00004550 || machine!=0x8664 || optionalSize<60 || magic!=0x20b ||
       timestamp!=clientTimestamp || imageSize!=clientImageSize)return 0;
    return namedFlag(base+recordRva);
}
}
