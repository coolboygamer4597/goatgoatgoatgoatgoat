#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SessionPresence {
inline constexpr std::array<std::uint64_t,12> watchedIds={3951468590ULL,167116ULL,34460022ULL,841001359ULL,207428722ULL,1846148282ULL,106663138ULL,462523610ULL,110758492ULL,1398405129ULL,4617335260ULL,2407806135ULL};
struct Player {std::uint64_t userId=0;std::string username,team;};
struct Notice {std::uint64_t sequence=0,userId=0;std::string username;};
inline bool Watched(std::uint64_t id){return std::find(watchedIds.begin(),watchedIds.end(),id)!=watchedIds.end();}
inline bool SpectatorTeam(std::string name){
    const auto first=name.find_first_not_of(" \t\r\n"),last=name.find_last_not_of(" \t\r\n");
    if(first==std::string::npos)return false;
    name=name.substr(first,last-first+1);
    for(char& c:name)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return name=="spectator"||name=="spectators";
}
class State {
    bool notificationsEnabled_=true;
    std::uint64_t session_=0,nextSequence_=0;
    std::unordered_map<std::uint64_t,double> present_;
    std::vector<Notice> notices_;
    int spectators_=0;
    bool available_=false;
public:
    void Configure(bool enabled){
        if(notificationsEnabled_==enabled)return;
        notificationsEnabled_=enabled;
        present_.clear();
        if(!enabled)notices_.clear();
    }
    void Update(std::uint64_t session,const std::vector<Player>& players,bool available,double now){
        available_=available;
        if(!available)return;
        if(session_!=session){present_.clear();session_=session;}
        spectators_=0;
        std::unordered_set<std::uint64_t> unique;
        for(const auto& player:players){
            if(!player.userId||!unique.insert(player.userId).second)continue;
            if(SpectatorTeam(player.team))++spectators_;
            if(!notificationsEnabled_||!Watched(player.userId))continue;
            auto pending=std::find_if(notices_.begin(),notices_.end(),[&](const Notice& n){return n.userId==player.userId;});
            if(pending!=notices_.end()&&!player.username.empty())pending->username=player.username;
            if(present_.find(player.userId)==present_.end()&&pending==notices_.end())
                notices_.push_back({++nextSequence_,player.userId,player.username.empty()?"User #"+std::to_string(player.userId):player.username});
            present_[player.userId]=now;
        }
        for(auto it=present_.begin();it!=present_.end();){
            if(unique.find(it->first)==unique.end()&&now-it->second>=2.0)it=present_.erase(it);
            else ++it;
        }
    }
    void Dismiss(std::uint64_t sequence){
        notices_.erase(std::remove_if(notices_.begin(),notices_.end(),[&](const Notice& n){return n.sequence==sequence;}),notices_.end());
    }
    bool Available()const{return available_;}
    int Spectators()const{return spectators_;}
    const std::vector<Notice>& Notices()const{return notices_;}
};
inline State state;
}
