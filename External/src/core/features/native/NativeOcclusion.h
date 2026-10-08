#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>
#include <optional>

namespace NativeOcclusion {
inline constexpr char flagName[]="RenderFastClusterOcclusionCulling";
struct Entry {std::uint64_t value,name,length;};
struct Section {std::uint64_t begin,size;std::uint32_t flags;};
struct Layout {std::uint64_t nameOffset=8,lengthOffset=16;};
struct Discovery {std::uint64_t address=0;Layout layout{};};
inline bool validLayout(Layout layout) {
    return layout.nameOffset>=8 && layout.nameOffset<=64 && !(layout.nameOffset&7) &&
        layout.lengthOffset>=8 && layout.lengthOffset<=72 && !(layout.lengthOffset&7) &&
        layout.nameOffset!=layout.lengthOffset;
}

template<class Read>
bool namedFlag(std::uint64_t base,std::uint64_t size,std::uint64_t flag,Read read,Layout layout={}) {
    auto inside=[&](std::uint64_t p,std::uint64_t n){return p>=base && n<=size && p-base<=size-n;};
    std::uint8_t value=0xff;std::uint64_t pointer=0,length=0;char name[sizeof(flagName)]{};
    const auto span=std::max(layout.nameOffset,layout.lengthOffset)+8;
    return validLayout(layout) && inside(flag,span) && read(flag,&value,sizeof(value)) && value<=1 &&
        read(flag+layout.nameOffset,&pointer,sizeof(pointer)) &&
        read(flag+layout.lengthOffset,&length,sizeof(length)) && length==sizeof(flagName)-1 &&
        inside(pointer,sizeof(name)) && read(pointer,name,sizeof(name)) &&
        std::memcmp(name,flagName,sizeof(name))==0;
}

template<class Read>
std::optional<Layout> measureLayout(std::uint64_t base,std::uint64_t size,std::uint64_t flag,Read read) {
    std::optional<Layout> result;
    for(std::uint64_t offset=8;offset<=64;offset+=8) {
        Layout layout{offset,offset+8};
        if(!namedFlag(base,size,flag,read,layout))continue;
        if(result)return {};
        result=layout;
    }
    return result;
}

template<class Read>
std::uint64_t resolve(std::uint64_t base,std::uint64_t size,Read read,Layout layout={}) {
    if(!base || size<4096 || size>512ULL*1024*1024 || size>~std::uint64_t(0)-base || !validLayout(layout))return 0;
    const auto metadataSpan=std::max(layout.nameOffset,layout.lengthOffset)+8;
    auto inside=[&](std::uint64_t p,std::uint64_t n){return p>=base && n<=size && p-base<=size-n;};
    unsigned char dos[64]{};
    if(!read(base,dos,sizeof(dos)) || dos[0]!='M' || dos[1]!='Z')return 0;
    std::uint32_t ntRva=0;std::memcpy(&ntRva,dos+60,4);
    if(ntRva<64 || ntRva>size-84)return 0;
    unsigned char nt[84]{};if(!read(base+ntRva,nt,sizeof(nt)))return 0;
    std::uint32_t signature=0,imageSize=0;std::uint16_t machine=0,count=0,optSize=0,magic=0;
    std::memcpy(&signature,nt,4);std::memcpy(&machine,nt+4,2);std::memcpy(&count,nt+6,2);
    std::memcpy(&optSize,nt+20,2);std::memcpy(&magic,nt+24,2);std::memcpy(&imageSize,nt+80,4);
    if(signature!=0x4550 || machine!=0x8664 || magic!=0x20b || optSize<112 || optSize>4096 ||
       !count || count>96 || imageSize!=size)return 0;
    const auto table=base+ntRva+24+optSize;
    std::vector<unsigned char> headers(count*40);
    if(!inside(table,headers.size()) || !read(table,headers.data(),headers.size()))return 0;
    std::vector<Section> sections;
    for(std::size_t i=0;i<count;++i) {
        std::uint32_t span=0,rva=0,flags=0;
        std::memcpy(&span,headers.data()+i*40+8,4);std::memcpy(&rva,headers.data()+i*40+12,4);
        std::memcpy(&flags,headers.data()+i*40+36,4);
        if(!span || !(flags&0x40000000) || (flags&0x20000000))continue;
        if(!inside(base+rva,span))return 0;
        sections.push_back({base+rva,span,flags});
    }
    auto scan=[&](const Section& s,auto visit) {
        constexpr std::size_t chunk=1024*1024;
        for(std::uint64_t off=0;off<s.size;off+=chunk) {
            const auto bytes=static_cast<std::size_t>(std::min<std::uint64_t>(chunk+64,s.size-off));
            std::vector<unsigned char> buffer(bytes,0);
            const auto at=s.begin+off;
            if(!read(at,buffer.data(),bytes)) {
                for(std::size_t p=0;p<bytes;p+=4096) {
                    const auto n=std::min<std::size_t>(4096,bytes-p);
                    if(!read(at+p,buffer.data()+p,n))std::memset(buffer.data()+p,0,n);
                }
            }
            visit(at,buffer);
        }
    };
    std::vector<std::uint64_t> names;
    for(const auto& s:sections)scan(s,[&](std::uint64_t at,const std::vector<unsigned char>& buffer){
        for(std::size_t i=0;i+sizeof(flagName)<=buffer.size();++i)
            if(buffer[i]==flagName[0] && std::memcmp(buffer.data()+i,flagName,sizeof(flagName))==0 &&
               std::find(names.begin(),names.end(),at+i)==names.end())names.push_back(at+i);
    });
    if(names.empty() || names.size()>16)return 0;
    std::vector<std::uint64_t> candidates;
    for(const auto& s:sections) {
        if(!(s.flags&0x80000000))continue;
        scan(s,[&](std::uint64_t at,const std::vector<unsigned char>& buffer){
            for(std::size_t i=static_cast<std::size_t>((8-(at&7))&7);i+8<=buffer.size();i+=8) {
                std::uint64_t p=0;std::memcpy(&p,buffer.data()+i,8);
                if(std::find(names.begin(),names.end(),p)==names.end() || at+i<s.begin+layout.nameOffset)continue;
                const auto flag=at+i-layout.nameOffset;
                if(flag<s.begin || metadataSpan>s.size || flag-s.begin>s.size-metadataSpan ||
                   !namedFlag(base,size,flag,read,layout))continue;
                if(std::find(candidates.begin(),candidates.end(),flag)==candidates.end())candidates.push_back(flag);
            }
        });
    }
    return candidates.size()==1?candidates[0]:0;
}

template<class Read>
Discovery discover(std::uint64_t base,std::uint64_t size,Read read,std::uint64_t anchor=0) {
    if(anchor) {
        const auto layout=measureLayout(base,size,anchor,read);
        if(layout && resolve(base,size,read,*layout)==anchor)return {anchor,*layout};
        return {};
    }
    const auto found=resolve(base,size,read);
    if(found) {
        const auto layout=measureLayout(base,size,found,read);
        if(layout)return {found,*layout};
    }
    return {};
}
}
