#include "../../features/TargetLabels.h"
#pragma once
#include "../../variables/variables.h"
#include "../../variables/player_rules.h"
#include "../skins/skins.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <vector>
#include <cmath>
#include <iomanip>
#include <chrono>
namespace Preferences {
inline bool saveTabs[4]={false,false,false,false};
inline std::string lastWritten, error;
inline bool loaded=false;
inline std::filesystem::path Path(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);return std::filesystem::path(p).parent_path()/L"goatgoatgoat.preferences";}
struct Field { int tab; std::string name; std::function<std::string()> get; std::function<void(const std::string&)> set; };
template<class T> void Add(std::vector<Field>& fields,int tab,const std::string& name,T& value){
 fields.push_back({tab,name,[&value]{std::ostringstream s;s<<std::setprecision(9)<<value;return s.str();},
 [&value](const std::string& text){T next{};std::istringstream s(text);if(s>>next && s.eof() && std::isfinite(static_cast<double>(next)))value=next;}});
}
inline std::vector<Field>& Fields(){static auto fields=[] {std::vector<Field> out;
        Add(out,0,"Aimbot.recoilEnabled",variables::Aimbot::recoilEnabled);
        Add(out,0,"Aimbot.recoilStrength",variables::Aimbot::recoilStrength);
        Add(out,0,"Aimbot.recoilPull",variables::Aimbot::recoilPull);
        Add(out,0,"Aimbot.recoilGlockStrength",variables::Aimbot::recoilGlockStrength);
        Add(out,0,"Aimbot.recoilGlockPull",variables::Aimbot::recoilGlockPull);
        Add(out,0,"Aimbot.silentEnabled",variables::Aimbot::silentEnabled);
        Add(out,0,"Aimbot.silentMethod",variables::Aimbot::silentMethod);
        Add(out,0,"Aimbot.silentKey",variables::Aimbot::silentKey);
        Add(out,0,"Aimbot.silentToggleKey",variables::Aimbot::silentToggleKey);
        Add(out,0,"Aimbot.silentShowFOV",variables::Aimbot::silentShowFOV);
        Add(out,0,"Aimbot.silentFovRadius",variables::Aimbot::silentFovRadius);
        Add(out,0,"Aimbot.silentFovColor.x",variables::Aimbot::silentFovColor.x);
        Add(out,0,"Aimbot.silentFovColor.y",variables::Aimbot::silentFovColor.y);
        Add(out,0,"Aimbot.silentFovColor.z",variables::Aimbot::silentFovColor.z);
        Add(out,0,"Aimbot.silentFovColor.w",variables::Aimbot::silentFovColor.w);
        Add(out,0,"Aimbot.silentNearestPoint",variables::Aimbot::silentNearestPoint);
        Add(out,0,"Aimbot.deadCheck",variables::Aimbot::deadCheck);
        Add(out,0,"Aimbot.teamCheck",variables::Aimbot::teamCheck);
        Add(out,0,"Aimbot.invisibleCheck",variables::Aimbot::invisibleCheck);
        Add(out,0,"Aimbot.aimTarget",variables::Aimbot::aimTarget);
        Add(out,1,"ESP.meshChams",variables::ESP::meshChams);
        Add(out,1,"ESP.meshChamsLocal",variables::ESP::meshChamsLocal);
        Add(out,1,"ESP.meshChamsOpacity",variables::ESP::meshChamsOpacity);
        Add(out,1,"ESP.meshChamsOcclusion",variables::ESP::meshChamsOcclusion);
        Add(out,1,"ESP.meshChamsUnionWalls",variables::ESP::meshChamsUnionWalls);
        Add(out,1,"ESP.meshTeamCheck",variables::ESP::meshTeamCheck);
        Add(out,1,"ESP.meshDeadCheck",variables::ESP::meshDeadCheck);
        Add(out,1,"ESP.meshInvisibleCheck",variables::ESP::meshInvisibleCheck);
        Add(out,1,"ESP.meshTransparencyCheck",variables::ESP::meshTransparencyCheck);
        Add(out,1,"ESP.meshTransparencyMin",variables::ESP::meshTransparencyMin);
        Add(out,1,"ESP.meshTransparencyMax",variables::ESP::meshTransparencyMax);
        Add(out,1,"ESP.chamsFillColor.0",variables::ESP::chamsFillColor[0]);
        Add(out,1,"ESP.chamsFillColor.1",variables::ESP::chamsFillColor[1]);
        Add(out,1,"ESP.chamsFillColor.2",variables::ESP::chamsFillColor[2]);
        Add(out,1,"ESP.chamsFillColor.3",variables::ESP::chamsFillColor[3]);
        Add(out,1,"ESP.meshChamsOccludedColor.0",variables::ESP::meshChamsOccludedColor[0]);
        Add(out,1,"ESP.meshChamsOccludedColor.1",variables::ESP::meshChamsOccludedColor[1]);
        Add(out,1,"ESP.meshChamsOccludedColor.2",variables::ESP::meshChamsOccludedColor[2]);
        Add(out,1,"ESP.meshChamsOccludedColor.3",variables::ESP::meshChamsOccludedColor[3]);
        Add(out,1,"ESP.meshChamsOutline",variables::ESP::meshChamsOutline);
        Add(out,1,"ESP.meshChamsOutlineColor.0",variables::ESP::meshChamsOutlineColor[0]);
        Add(out,1,"ESP.meshChamsOutlineColor.1",variables::ESP::meshChamsOutlineColor[1]);
        Add(out,1,"ESP.meshChamsOutlineColor.2",variables::ESP::meshChamsOutlineColor[2]);
        Add(out,1,"ESP.meshChamsOutlineColor.3",variables::ESP::meshChamsOutlineColor[3]);
        Add(out,1,"ESP.meshChamsOutlineFade",variables::ESP::meshChamsOutlineFade);
        Add(out,1,"ESP.meshChamsOutlineStyle",variables::ESP::meshChamsOutlineStyle);
        Add(out,1,"ESP.meshChamsDxMode",variables::ESP::meshChamsDxMode);
        Add(out,1,"ESP.meshChamsOccludedDxMode",variables::ESP::meshChamsOccludedDxMode);
        Add(out,1,"ESP.visualKeybindEnabled",variables::ESP::visualKeybindEnabled);
        Add(out,1,"ESP.visualKeybindKey",variables::ESP::visualKeybindKey);
        Add(out,1,"ESP.visualKeybindMode",variables::ESP::visualKeybindMode);
        Add(out,1,"ESP.visualKeybindTarget",variables::ESP::visualKeybindTarget);
        Add(out,1,"ESP.nativeUnlimited",variables::ESP::nativeUnlimited);
        Add(out,1,"ESP.meshUnlimited",variables::ESP::meshUnlimited);
        Add(out,1,"ESP.nativeDistance",variables::ESP::nativeDistance);
        Add(out,1,"ESP.meshDistance",variables::ESP::meshDistance);
        Add(out,3,"Weapons.native",variables::Weapons::native);
        Add(out,3,"Weapons.preview",variables::Weapons::preview);

        Add(out,3,"Weapons.showOriginal.v2",variables::Weapons::showOriginal);
        for(int group=0;group<2;++group){
            auto& m=group?variables::Weapons::gloves:variables::Weapons::arms;
            const std::string key=group?"Weapons.gloves.":"Weapons.arms.";
            Add(out,3,key+"enabled",m.enabled);Add(out,3,key+"style",m.style);
            Add(out,3,key+"opacity",m.opacity);Add(out,3,key+"speed",m.speed);Add(out,3,key+"scale",m.scale);
            Add(out,3,key+"glow",m.glow);Add(out,3,key+"glowStrength",m.glowStrength);
            for(int i=0;i<4;++i){Add(out,3,key+"color."+std::to_string(i),m.color[i]);Add(out,3,key+"glowColor."+std::to_string(i),m.glowColor[i]);}
        }

        Add(out,3,"Weapons.viewmodel",variables::Weapons::viewmodel);
        Add(out,3,"Weapons.ar15",variables::Weapons::ar15);
        Add(out,3,"Weapons.glock",variables::Weapons::glock);
        Add(out,3,"Weapons.glow",variables::Weapons::glow);
        Add(out,3,"Weapons.opacity",variables::Weapons::opacity);
        Add(out,3,"Weapons.speed.v2",variables::Weapons::speed);
        Add(out,3,"Weapons.scale.v2",variables::Weapons::scale);
        Add(out,3,"Weapons.glowStrength",variables::Weapons::glowStrength);
        Add(out,3,"Weapons.nativeStyle",variables::Weapons::nativeStyle);
        Add(out,3,"Weapons.color.0",variables::Weapons::color[0]);
        Add(out,3,"Weapons.color.1",variables::Weapons::color[1]);
        Add(out,3,"Weapons.color.2",variables::Weapons::color[2]);
        Add(out,3,"Weapons.color.3",variables::Weapons::color[3]);
        Add(out,3,"Weapons.glowColor.0",variables::Weapons::glowColor[0]);
        Add(out,3,"Weapons.glowColor.1",variables::Weapons::glowColor[1]);
        Add(out,3,"Weapons.glowColor.2",variables::Weapons::glowColor[2]);
        Add(out,3,"Weapons.glowColor.3",variables::Weapons::glowColor[3]);
        Add(out,1,"ESP.nativeChams",variables::ESP::nativeChams);
        Add(out,1,"ESP.nativeChamsStyle",variables::ESP::nativeChamsStyle);
        Add(out,1,"ESP.nativeChamsOpacity",variables::ESP::nativeChamsOpacity);
        Add(out,1,"ESP.nativeChamsOnly",variables::ESP::nativeChamsOnly);
        Add(out,1,"ESP.nativeChamsOcclusion",variables::ESP::nativeChamsOcclusion);
        Add(out,1,"ESP.nativeChamsOccludedStyle",variables::ESP::nativeChamsOccludedStyle);
        Add(out,1,"ESP.nativeChamsOccludedColor.0",variables::ESP::nativeChamsOccludedColor[0]);
        Add(out,1,"ESP.nativeChamsOccludedColor.1",variables::ESP::nativeChamsOccludedColor[1]);
        Add(out,1,"ESP.nativeChamsOccludedColor.2",variables::ESP::nativeChamsOccludedColor[2]);
        Add(out,1,"ESP.nativeChamsOccludedColor.3",variables::ESP::nativeChamsOccludedColor[3]);
        Add(out,1,"ESP.nativeChamsAnimationSpeed",variables::ESP::nativeChamsAnimationSpeed);
        Add(out,1,"ESP.nativePreview",variables::ESP::nativePreview);
        Add(out,1,"ESP.nativeChamsPatternSize",variables::ESP::nativeChamsPatternSize);
        Add(out,1,"ESP.nativeChamsColor.0",variables::ESP::nativeChamsColor[0]);
        Add(out,1,"ESP.nativeChamsColor.1",variables::ESP::nativeChamsColor[1]);
        Add(out,1,"ESP.nativeChamsColor.2",variables::ESP::nativeChamsColor[2]);
        Add(out,1,"ESP.nativeChamsColor.3",variables::ESP::nativeChamsColor[3]);
        Add(out,1,"ESP.nativeChamsGlow",variables::ESP::nativeChamsGlow);
        Add(out,1,"ESP.nativeChamsGlowColor.0",variables::ESP::nativeChamsGlowColor[0]);
        Add(out,1,"ESP.nativeChamsGlowColor.1",variables::ESP::nativeChamsGlowColor[1]);
        Add(out,1,"ESP.nativeChamsGlowColor.2",variables::ESP::nativeChamsGlowColor[2]);
        Add(out,1,"ESP.nativeChamsGlowColor.3",variables::ESP::nativeChamsGlowColor[3]);
        Add(out,1,"ESP.nativeChamsGlowStrength",variables::ESP::nativeChamsGlowStrength);
        Add(out,1,"ESP.nativeChamsWalls",variables::ESP::nativeChamsWalls);
        Add(out,2,"Misc.streamProof",variables::Misc::streamProof);
        Add(out,2,"Misc.spotifyPlayer",variables::Misc::spotifyPlayer);
        Add(out,2,"Misc.streamKey",variables::Misc::streamKey);
        Add(out,2,"Misc.streamKeyMode",variables::Misc::streamKeyMode);
        Add(out,2,"Misc.keybinds",variables::Misc::keybinds);
        Add(out,2,"Misc.spotifyKey",variables::Misc::spotifyKey);
        Add(out,2,"Misc.spotifyKeyMode",variables::Misc::spotifyKeyMode);
        Add(out,2,"Misc.fpsLimit",variables::Misc::fpsLimit);
        Add(out,-1,"Theme.preset",variables::Theme::preset);
        Add(out,-1,"Theme.sadblobStyle",variables::Theme::sadblobStyle);
        out.push_back({-1,"Theme.customMp4Path",
            [] { return variables::Theme::customMp4Path; },
            [](const std::string& path) { variables::Theme::customMp4Path = path; }});
        Add(out,-1,"Theme.background.x",variables::Theme::background.x);
        Add(out,-1,"Theme.background.y",variables::Theme::background.y);
        Add(out,-1,"Theme.background.z",variables::Theme::background.z);
        Add(out,-1,"Theme.background.w",variables::Theme::background.w);
        Add(out,-1,"Theme.panels.x",variables::Theme::panels.x);
        Add(out,-1,"Theme.panels.y",variables::Theme::panels.y);
        Add(out,-1,"Theme.panels.z",variables::Theme::panels.z);
        Add(out,-1,"Theme.panels.w",variables::Theme::panels.w);
        Add(out,-1,"Theme.controls.x",variables::Theme::controls.x);
        Add(out,-1,"Theme.controls.y",variables::Theme::controls.y);
        Add(out,-1,"Theme.controls.z",variables::Theme::controls.z);
        Add(out,-1,"Theme.controls.w",variables::Theme::controls.w);
        Add(out,-1,"Theme.accent.x",variables::Theme::accent.x);
        Add(out,-1,"Theme.accent.y",variables::Theme::accent.y);
        Add(out,-1,"Theme.accent.z",variables::Theme::accent.z);
        Add(out,-1,"Theme.accent.w",variables::Theme::accent.w);
        Add(out,-1,"Theme.text.x",variables::Theme::text.x);
        Add(out,-1,"Theme.text.y",variables::Theme::text.y);
        Add(out,-1,"Theme.text.z",variables::Theme::text.z);
        Add(out,-1,"Theme.text.w",variables::Theme::text.w);
        Add(out,-1,"Theme.textBright.x",variables::Theme::textBright.x);
        Add(out,-1,"Theme.textBright.y",variables::Theme::textBright.y);
        Add(out,-1,"Theme.textBright.z",variables::Theme::textBright.z);
        Add(out,-1,"Theme.textBright.w",variables::Theme::textBright.w);
        Add(out,2,"Menu.key",variables::menuKey);
        Add(out,2,"Misc.performancePanel",variables::Misc::performancePanel);
        Add(out,2,"Misc.keybindPanelX",variables::Misc::keybindPanelX);
        Add(out,2,"Misc.keybindPanelY",variables::Misc::keybindPanelY);
        Add(out,2,"Misc.performancePanelX",variables::Misc::performancePanelX);
        Add(out,2,"Misc.performancePanelY",variables::Misc::performancePanelY);
        Add(out,3,"Extra.1",variables::Weapons::hide);
        Add(out,1,"Extra.6",TargetLabels::enabled[0]);
        Add(out,1,"Extra.7",TargetLabels::sizes[0]);
        Add(out,1,"Extra.8",TargetLabels::positions[0]);
        Add(out,1,"Extra.9",TargetLabels::colors[0][0]);
        Add(out,1,"Extra.10",TargetLabels::colors[0][1]);
        Add(out,1,"Extra.11",TargetLabels::colors[0][2]);
        Add(out,1,"Extra.12",TargetLabels::colors[0][3]);
        Add(out,1,"Extra.13",TargetLabels::enabled[1]);
        Add(out,1,"Extra.14",TargetLabels::sizes[1]);
        Add(out,1,"Extra.15",TargetLabels::positions[1]);
        Add(out,1,"Extra.16",TargetLabels::colors[1][0]);
        Add(out,1,"Extra.17",TargetLabels::colors[1][1]);
        Add(out,1,"Extra.18",TargetLabels::colors[1][2]);
        Add(out,1,"Extra.19",TargetLabels::colors[1][3]);
        Add(out,1,"Extra.20",TargetLabels::enabled[2]);
        Add(out,1,"Extra.21",TargetLabels::sizes[2]);
        Add(out,1,"Extra.22",TargetLabels::positions[2]);
        Add(out,1,"Extra.23",TargetLabels::colors[2][0]);
        Add(out,1,"Extra.24",TargetLabels::colors[2][1]);
        Add(out,1,"Extra.25",TargetLabels::colors[2][2]);
        Add(out,1,"Extra.26",TargetLabels::colors[2][3]);
        return out;}();return fields;}
inline std::string Serialize(){
 std::ostringstream out;out<<"format=1\n";
 for(int i=0;i<4;++i)out<<"Save."<<i<<"="<<saveTabs[i]<<"\n";
 for(auto& f:Fields())if(f.tab<0 || saveTabs[f.tab])out<<f.name<<"="<<f.get()<<"\n";
 if(saveTabs[2])for(const auto& p:PlayerRules::rules)if(p.second.aim||p.second.esp)
  out<<"Player."<<p.first<<"="<<p.second.aim<<" "<<p.second.esp<<"\n";
 if(saveTabs[3])out<<"Skin.selection="<<Skins::SavedSelection()<<"\n";
 return out.str();
}
inline void Apply(const std::string& text){
 std::map<std::string,std::string> values;std::istringstream in(text);std::string line;
 while(std::getline(in,line)){auto p=line.find('=');if(p!=std::string::npos)values[line.substr(0,p)]=line.substr(p+1);}
 for(int i=0;i<4;++i)saveTabs[i]=values["Save."+std::to_string(i)]=="1";
 for(auto& f:Fields()){auto it=values.find(f.name);if(it!=values.end() && (f.tab<0||saveTabs[f.tab]))f.set(it->second);}
 if(saveTabs[2])for(const auto& p:values)if(p.first.rfind("Player.",0)==0){
  std::istringstream idText(p.first.substr(7));std::uint64_t id=0;PlayerRules::Rule rule;
  std::istringstream flags(p.second);if(idText>>id && id && flags>>rule.aim>>rule.esp)PlayerRules::rules[id]=rule;
 }
 if(saveTabs[3]){auto it=values.find("Skin.selection");if(it!=values.end())Skins::RestoreSelection(it->second);}
 variables::Theme::preset=(std::max)(0,(std::min)(16,variables::Theme::preset));
 variables::Theme::sadblobStyle=(std::max)(0,(std::min)(6,variables::Theme::sadblobStyle));
 variables::menuKey=variables::menuKey>0&&variables::menuKey<1024?variables::menuKey:VK_INSERT;
 variables::Aimbot::recoilStrength=std::clamp(variables::Aimbot::recoilStrength,0.f,1.f);
 variables::Aimbot::recoilPull=std::clamp(variables::Aimbot::recoilPull,0.f,1200.f);
 variables::Aimbot::recoilGlockStrength=std::clamp(variables::Aimbot::recoilGlockStrength,0.f,1.f);
 variables::Aimbot::recoilGlockPull=std::clamp(variables::Aimbot::recoilGlockPull,0.f,160.f);
 variables::Aimbot::silentMethod=(std::max)(1,(std::min)(2,variables::Aimbot::silentMethod));
 variables::Aimbot::aimTarget=(std::max)(0,(std::min)(7,variables::Aimbot::aimTarget));
 variables::Aimbot::silentFovRadius=(std::max)(5.f,(std::min)(600.f,variables::Aimbot::silentFovRadius));
 variables::ESP::visualKeybindMode=variables::ESP::visualKeybindMode==1?1:0;
 variables::ESP::visualKeybindTarget=variables::ESP::visualKeybindTarget==1?1:0;
 variables::Misc::fpsLimit=(std::max)(60,(std::min)(2000,variables::Misc::fpsLimit));
}
inline void Load(const std::filesystem::path& path=Path()){std::ifstream f(path);std::string text((std::istreambuf_iterator<char>(f)),{});Apply(text);lastWritten=text;loaded=true;}
inline bool Save(const std::filesystem::path& path=Path()){
 if(!loaded)return false;
 auto text=Serialize();if(text==lastWritten)return true;
 auto p=path,tmp=p;tmp+=L".tmp";
 {std::ofstream f(tmp,std::ios::binary|std::ios::trunc);f<<text;f.flush();if(!f){error="Could not save settings";return false;}}
 if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="Could not save settings";return false;}
 lastWritten=std::move(text);error.clear();return true;
}
inline void Tick(){static auto next=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::milliseconds(500);Save();}
}
