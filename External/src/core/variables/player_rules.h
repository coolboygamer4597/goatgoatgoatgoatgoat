#pragma once
#include <map>
#include <cstdint>
namespace PlayerRules {
struct Rule { bool aim=false, esp=false; };
inline std::map<std::uint64_t, Rule> rules;
inline bool Aim(std::uint64_t id) { auto it=rules.find(id); return id && it!=rules.end() && it->second.aim; }
inline bool Esp(std::uint64_t id) { auto it=rules.find(id); return id && it!=rules.end() && it->second.esp; }
}
