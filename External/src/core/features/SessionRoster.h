#pragma once
#include "SessionPresence.h"
#include "../globals/globals.h"
#include "../variables/variables.h"
#include <chrono>

namespace SessionRoster {
inline std::chrono::steady_clock::time_point nextRefresh;
inline std::uintptr_t playersAddress=0,localAddress=0;
inline std::uint64_t generation=0;
inline void Tick(){
    SessionPresence::state.Configure(variables::Misc::moderatorAlerts);
    if(!variables::Misc::moderatorAlerts&&!variables::Misc::spectatorCounter)return;
    const auto now=std::chrono::steady_clock::now();
    const auto players=Globals::players.Addr,local=Globals::localPlayer.Addr;
    if(players!=playersAddress||local!=localAddress){
        playersAddress=players;localAddress=local;++generation;nextRefresh={};
    }
    if(now<nextRefresh)return;
    nextRefresh=now+std::chrono::milliseconds(500);
    const double seconds=std::chrono::duration<double>(now.time_since_epoch()).count();
    std::vector<SessionPresence::Player> roster;
    bool complete=memory&&memory->IsConnected()&&players&&local,hasLocal=false;
    if(complete){
        std::unordered_map<std::uintptr_t,std::string> teams;
        for(const auto& player:Globals::players.GetChildList()){
            if(player.GetClass()!="Player")continue;
            const auto id=memory->read<std::uint64_t>(player.Addr+Offsets::Player::UserId);
            if(!id){complete=false;continue;}
            hasLocal=hasLocal||player.Addr==local;
            const auto team=memory->read<std::uintptr_t>(player.Addr+Offsets::Player::Team);
            auto found=teams.find(team);
            if(found==teams.end())found=teams.emplace(team,team?RBX::RbxInstance(team).GetName():std::string{}).first;
            roster.push_back({id,player.GetName(),found->second});
        }
    }
    SessionPresence::state.Update(generation,roster,complete&&hasLocal,seconds);
}
}
