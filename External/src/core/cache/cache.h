#pragma once
#include "../../../src/sdk/sdk.h"
#include "../globals/globals.h"
#include "../variables/variables.h"
#include "../keys/keys.h"
#include "../variables/player_rules.h"
#include <vector>
#include <string>
#include <utility>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace PlayerCache {
struct CachedPlayer {
    std::uint64_t userId = 0;
    std::uintptr_t playerAddr = 0;
    std::uintptr_t characterAddr = 0;
    std::uintptr_t humanoidAddr = 0;
    std::uintptr_t rootPartAddr = 0;
    std::uintptr_t headAddr = 0;
    std::uintptr_t teamAddr = 0;
    std::string name;
    std::string tool = "None";
    RBX::Vec3 position{};
    float health = 0.0f;
    float distance = 0.0f;
    bool isValid = false;
    bool isR6 = false;
};

struct LimbAddrs {
    ULONGLONG collectedAt = 0;
    bool r6 = false;
    std::uintptr_t head = 0;
    std::uintptr_t hrp = 0;
    std::uintptr_t torso = 0;
    std::uintptr_t upperTorso = 0;
    std::uintptr_t lowerTorso = 0;
    std::uintptr_t lUpperArm = 0;
    std::uintptr_t lLowerArm = 0;
    std::uintptr_t lHand = 0;
    std::uintptr_t rUpperArm = 0;
    std::uintptr_t rLowerArm = 0;
    std::uintptr_t rHand = 0;
    std::uintptr_t lUpperLeg = 0;
    std::uintptr_t lLowerLeg = 0;
    std::uintptr_t lFoot = 0;
    std::uintptr_t rUpperLeg = 0;
    std::uintptr_t rLowerLeg = 0;
    std::uintptr_t rFoot = 0;
    std::uintptr_t lArm = 0;
    std::uintptr_t rArm = 0;
    std::uintptr_t lLeg = 0;
    std::uintptr_t rLeg = 0;
    std::uintptr_t humanoid = 0;
};

inline std::unordered_map<std::uintptr_t, LimbAddrs> limbCache;

inline const LimbAddrs& GetLimbs(std::uintptr_t characterAddr) {
    auto it = limbCache.find(characterAddr);
    const ULONGLONG now = GetTickCount64();
    if (it != limbCache.end() && now - it->second.collectedAt < 1000 + ((characterAddr >> 4) % 500))
        return it->second;
    if (it != limbCache.end())
        limbCache.erase(it);
    if (limbCache.size() >= 256)
        limbCache.clear();
    LimbAddrs l{};
    l.collectedAt = now;
    RBX::RbxInstance ch{characterAddr};

    for (const auto& child : ch.GetChildList()) {
        const auto name = child.GetName();
        if (name == "Head") l.head = child.Addr;
        else if (name == "HumanoidRootPart") l.hrp = child.Addr;
        else if (name == "Torso") l.torso = child.Addr;
        else if (name == "Left Arm") l.lArm = child.Addr;
        else if (name == "Right Arm") l.rArm = child.Addr;
        else if (name == "Left Leg") l.lLeg = child.Addr;
        else if (name == "Right Leg") l.rLeg = child.Addr;
        else if (name == "UpperTorso") l.upperTorso = child.Addr;
        else if (name == "LowerTorso") l.lowerTorso = child.Addr;
        else if (name == "LeftUpperArm") l.lUpperArm = child.Addr;
        else if (name == "LeftLowerArm") l.lLowerArm = child.Addr;
        else if (name == "LeftHand") l.lHand = child.Addr;
        else if (name == "RightUpperArm") l.rUpperArm = child.Addr;
        else if (name == "RightLowerArm") l.rLowerArm = child.Addr;
        else if (name == "RightHand") l.rHand = child.Addr;
        else if (name == "LeftUpperLeg") l.lUpperLeg = child.Addr;
        else if (name == "LeftLowerLeg") l.lLowerLeg = child.Addr;
        else if (name == "LeftFoot") l.lFoot = child.Addr;
        else if (name == "RightUpperLeg") l.rUpperLeg = child.Addr;
        else if (name == "RightLowerLeg") l.rLowerLeg = child.Addr;
        else if (name == "RightFoot") l.rFoot = child.Addr;
        else if (child.GetClass() == "Humanoid") l.humanoid = child.Addr;
    }
    l.r6 = l.torso != 0;
    auto res = limbCache.emplace(characterAddr, l);
    return res.first->second;
}

inline void PruneLimbs(const std::unordered_set<std::uintptr_t>& alive) {
    for (auto it = limbCache.begin(); it != limbCache.end();) {
        if (alive.find(it->first) == alive.end())
            it = limbCache.erase(it);
        else
            ++it;
    }
}

inline std::vector<CachedPlayer> players;
inline RBX::Vec3 localPlayerPos{};
inline std::uintptr_t localPlayerTeam = 0;

inline std::string localPlayerTeamName;
inline std::uintptr_t localRootPrim = 0;

inline void updateplayers() {
    if (!Globals::players.Addr || !Globals::localPlayer.Addr) {
        players.clear();
        localRootPrim = 0;
        return;
    }
    using Clock = std::chrono::steady_clock;
    static auto lastUpdate = Clock::now() - std::chrono::seconds(10);
    if (Clock::now() - lastUpdate < std::chrono::milliseconds(100))
        return;
    lastUpdate = Clock::now();
    if (localRootPrim) {
        localPlayerPos = memory->read<RBX::Vec3>(localRootPrim + Offsets::Primitive::Position);
        for (auto& c : players) {
            if (!c.isValid)
                continue;
            c.health = memory->read<float>(c.humanoidAddr + Offsets::Humanoid::Health);

        }
    }
    auto localChar = Globals::localPlayer.GetModelRef();
    if (!localChar.Addr) {
        players.clear();
        localRootPrim = 0;
        return;
    }
    auto localRoot = localChar.FindChild("HumanoidRootPart");
    if (!localRoot.Addr) {
        players.clear();
        localRootPrim = 0;
        return;
    }
    localRootPrim = localRoot.GetPrimitivePtr();
    localPlayerPos = localRoot.GetPos();
    localPlayerTeam = memory->read<std::uintptr_t>(Globals::localPlayer.Addr + Offsets::Player::Team);
    localPlayerTeamName = localPlayerTeam
        ? RBX::RbxInstance(localPlayerTeam).GetName() : std::string{};

    auto list = Globals::players.GetChildList();
    std::unordered_set<std::uintptr_t> alive;
    alive.reserve(list.size() + 1);
    alive.insert(localChar.Addr);

    for (auto& plr : players)
        plr.isValid = false;

    for (auto& plr : list) {
        if (plr.Addr == Globals::localPlayer.Addr)
            continue;
        const auto character = plr.GetModelRef();
        if (!character.Addr)
            continue;
        alive.insert(character.Addr);
        CachedPlayer* slot = nullptr;
        for (auto& c : players) {
            if (c.playerAddr == plr.Addr) {
                slot = &c;
                break;
            }
        }
        if (slot && slot->characterAddr == character.Addr) {

            if (RBX::RbxInstance(slot->rootPartAddr).GetParent().Addr != character.Addr ||
                RBX::RbxInstance(slot->humanoidAddr).GetParent().Addr != character.Addr)
                limbCache.erase(character.Addr);
            const auto& fresh = GetLimbs(character.Addr);
            if (!fresh.hrp || !fresh.humanoid)
                continue;
            slot->humanoidAddr = fresh.humanoid;
            slot->rootPartAddr = fresh.hrp;
            slot->headAddr = fresh.head;
            slot->isR6 = fresh.r6;
            slot->health = memory->read<float>(fresh.humanoid + Offsets::Humanoid::Health);
            slot->teamAddr = memory->read<std::uintptr_t>(plr.Addr + Offsets::Player::Team);
            slot->isValid = true;
            continue;
        }
        const auto& limbs = GetLimbs(character.Addr);
        if (!limbs.hrp || !limbs.humanoid)
            continue;
        const float hp = memory->read<float>(limbs.humanoid + Offsets::Humanoid::Health);
        const auto team = memory->read<std::uintptr_t>(plr.Addr + Offsets::Player::Team);
        CachedPlayer c{};
        c.playerAddr = plr.Addr;
        c.userId = memory->read<std::uint64_t>(plr.Addr + Offsets::Player::UserId);
        c.characterAddr = character.Addr;
        c.humanoidAddr = limbs.humanoid;
        c.rootPartAddr = limbs.hrp;
        c.headAddr = limbs.head;
        c.teamAddr = team;
        c.name = plr.GetName();
        c.health = hp;
        c.isR6 = limbs.r6;
        c.isValid = true;
        if (slot)
            *slot = std::move(c);
        else
            players.push_back(std::move(c));
    }

    players.erase(std::remove_if(players.begin(), players.end(), [](const CachedPlayer& c) { return !c.isValid; }), players.end());
    PruneLimbs(alive);
}
}
