#pragma once
#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

class AvatarOccluderFilter {
public:
    std::unordered_set<std::uint64_t> roots;
    template<class Parent> bool Excludes(std::uint64_t part,Parent parent) {
        std::array<std::uint64_t,32> chain{};unsigned count=0;bool result=false;
        while(part && count<chain.size()) {
            if(roots.count(part)){result=true;break;}
            const auto known=memo.find(part);if(known!=memo.end()){result=known->second;break;}
            bool cycle=false;for(unsigned i=0;i<count;++i)if(chain[i]==part){cycle=true;break;}
            if(cycle)break;
            chain[count++]=part;part=parent(part);
        }
        for(unsigned i=0;i<count;++i)memo[chain[i]]=result;
        return result;
    }
private:
    std::unordered_map<std::uint64_t,bool> memo;
};
