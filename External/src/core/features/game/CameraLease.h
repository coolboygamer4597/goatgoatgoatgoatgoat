#pragma once
#include "OwnedValue.h"
#include <cstdint>
namespace StretchResolution {
struct Owner {std::uint32_t pid=0;std::uintptr_t model=0,workspace=0,camera=0;bool operator==(const Owner& b)const{return pid==b.pid&&model==b.model&&workspace==b.workspace&&camera==b.camera;}bool operator!=(const Owner& b)const{return !(*this==b);}};
template<class T> struct Lease {
    Owner owner{};GameVisuals::OwnedValue<T> value;
    template<class Fresh,class Read,class Write>bool Update(bool enabled,const T& wanted,Fresh fresh,Read read,Write write){
        const Owner current=fresh();if(!current.camera){owner={};value={};return false;}
        if(current!=owner){owner=current;value={};}
        auto guardedRead=[&](T& out){return fresh()==owner&&read(owner,out)&&fresh()==owner;};
        auto guardedWrite=[&](const T& out){return fresh()==owner&&write(owner,out);};
        return enabled?value.Apply(wanted,guardedRead,guardedWrite):value.Restore(guardedRead,guardedWrite);
    }
};
}
