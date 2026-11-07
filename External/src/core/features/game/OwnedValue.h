#pragma once
#include <cstring>
namespace GameVisuals {
template<class T> struct OwnedValue {
    bool owned=false;
    T before{},after{};
    template<class Read,class Write> bool Apply(const T& desired,Read read,Write write){
        T current{};if(!read(current))return false;
        if(!owned||std::memcmp(&current,&after,sizeof(T)))before=current;
        if(!std::memcmp(&current,&desired,sizeof(T))){if(!owned)return true;after=desired;return true;}
        if(!write(desired))return false;
        owned=true;after=desired;return true;
    }
    template<class Read,class Write> bool Restore(Read read,Write write){
        if(!owned)return true;T current{};if(!read(current))return false;
        if(!std::memcmp(&current,&after,sizeof(T))&&!write(before))return false;
        owned=false;return true;
    }
};
}
