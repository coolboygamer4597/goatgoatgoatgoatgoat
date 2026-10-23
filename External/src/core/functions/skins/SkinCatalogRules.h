#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
namespace SkinCatalogRules {
inline bool Available(std::string_view skin){return skin!="A4"&&skin!="Blox"&&skin!="S1S1(old)";}
inline void Filter(std::vector<std::string>& skins){skins.erase(std::remove_if(skins.begin(),skins.end(),[](const std::string& skin){return !Available(skin);}),skins.end());}
inline std::string Selection(const std::string& skin){return Available(skin)?skin:"Default";}
}
