#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace WeaponOrnamentRuntime {
struct Layout{std::uintptr_t rate=0,texture=0,transparency=0,enabled=0,lifetime=0;explicit operator bool()const{return rate>=0x100&&rate<0x1000&&texture>=0x100&&texture<0x1000&&rate%4==0&&texture%8==0&&rate!=texture&&(!transparency||(transparency>=0x100&&transparency<0x1000&&transparency%4==0&&transparency!=rate&&transparency!=texture))&&(!enabled||(enabled>=0x100&&enabled<0x1000))&&(!lifetime||(lifetime>=0x100&&lifetime<0xff8&&lifetime%4==0));}};
inline Layout Resolve(const std::string& version,std::uintptr_t rate=0,std::uintptr_t texture=0,std::uintptr_t transparency=0,std::uintptr_t enabled=0,std::uintptr_t lifetime=0){
    if(version=="version-9ac8595712b447a7"){if(!rate)rate=0x230;if(!texture)texture=0x1a8;if(!transparency)transparency=0x228;if(!enabled)enabled=0x250;if(!lifetime)lifetime=0x1dc;}
    Layout result{rate,texture,transparency,enabled,lifetime};return result?result:Layout{};
}
inline std::uintptr_t OutlineField(const std::string& version,std::uintptr_t field=0){if(!field&&version=="version-9ac8595712b447a7")field=0xd4;return field>=0x80&&field<0x1000&&field%4==0?field:0;}
inline bool Glint(const std::string& name,const std::string& parent,const std::string& asset){return name=="Glint"&&parent=="GlintAttach"&&asset=="13279119039";}
inline bool GoldEffect(const std::string& name,const std::string& parent,const std::string& asset){return (name=="Dust"&&(parent=="Bolt"||parent=="handle1")&&asset=="7216854591")||(name=="FlashFX[Flash]"&&parent=="FirePart"&&asset=="9563725822");}
struct Candidate{std::uintptr_t address=0,parent=0,descriptor=0,field=0;bool hidden=false,transparency=false;};
struct Change{Candidate identity;float original=0;};
struct ByteChange{Candidate identity;std::uint8_t original=0;};
inline bool Same(const Candidate& a,const Candidate& b){return a.address==b.address&&a.parent==b.parent&&a.descriptor==b.descriptor&&a.field==b.field;}
template<class Read,class Write>void UpdateEnabled(const std::vector<Candidate>& current,std::vector<ByteChange>& changes,Read&& read,Write&& write){
    std::vector<ByteChange> next;
    for(const auto& item:current){
        std::uint8_t value=0;if(!read(item.field,value)||value>1)continue;
        const auto old=std::find_if(changes.begin(),changes.end(),[&](const ByteChange& c){return Same(item,c.identity);});
        if(item.hidden){if(old!=changes.end()&&!value){next.push_back(*old);continue;}if(value&&write(item.field,std::uint8_t(0)))next.push_back({item,value});}
        else if(old!=changes.end()&&!value&&!write(item.field,old->original))next.push_back(*old);
    }
    changes=std::move(next);
}
template<class Read,class Write>void Update(const std::vector<Candidate>& current,std::vector<Change>& changes,Read&& read,Write&& write){
    std::vector<Change> next;
    for(const auto& item:current){
        float rate=0;if(!read(item.field,rate)||!std::isfinite(rate)||rate<0||rate>(item.transparency?1.f:10000.f))continue;
        const float hiddenValue=item.transparency?1.f:0.f;
        const auto old=std::find_if(changes.begin(),changes.end(),[&](const Change& c){return Same(item,c.identity);});
        if(item.hidden){
            if(old!=changes.end()&&rate==hiddenValue){next.push_back(*old);continue;}
            if(rate!=hiddenValue&&write(item.field,hiddenValue))next.push_back({item,rate});
        }else if(old!=changes.end()&&rate==hiddenValue&&!write(item.field,old->original))next.push_back(*old);
    }
    changes=std::move(next);
}
}
