#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
namespace NativeCodePadding {
using Range=std::pair<std::uintptr_t,std::size_t>;
inline std::uintptr_t Fit(std::uintptr_t begin,std::uintptr_t end,std::size_t need,const std::vector<Range>& reserved){
    if(!need||end<=begin||need>end-begin||begin>~std::uintptr_t(0)-15)return 0;
    auto candidate=(begin+15)&~std::uintptr_t(15);
    for(std::size_t attempt=0;attempt<=reserved.size();++attempt){
        if(candidate>end||need>end-candidate)return 0;
        bool moved=false;
        for(const auto& range:reserved){
            if(!range.second||range.first>~std::uintptr_t(0)-range.second)continue;
            const auto limit=range.first+range.second;
            if(candidate<limit&&range.first<candidate+need){
                if(limit>~std::uintptr_t(0)-15)return 0;
                candidate=(limit+15)&~std::uintptr_t(15);moved=true;break;
            }
        }
        if(!moved)return candidate;
    }
    return 0;
}
}
