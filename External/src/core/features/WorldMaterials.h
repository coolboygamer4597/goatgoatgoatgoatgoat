#pragma once
#include "world_render/WorldRender.h"
#include "../cache/cache.h"
#include "../globals/globals.h"
#include "../../render/menu/library.h"
#include "native/NativeChams.h"
#include <unordered_set>
namespace WorldMaterials {
inline bool Available(){return false;}
inline world_render::Settings settings;
inline float speed=1,scale=3,opacity=.65f;
inline int effect=0,style=0;
inline std::vector<std::uintptr_t> parts;
inline std::size_t scanned=0;
inline void Tick(){
 if(!Available()){parts.clear();scanned=0;return;}

 static std::vector<RBX::RbxInstance> pending;
 static std::vector<std::uintptr_t> staging;
 static std::unordered_set<std::uintptr_t> visited,excluded;
 static auto next=std::chrono::steady_clock::time_point{};
 if(!settings.enabled){parts.clear();pending.clear();staging.clear();next={};return;}
 auto now=std::chrono::steady_clock::now();
 if(pending.empty()){
  if(now<next)return;
  staging.clear();visited.clear();excluded.clear();
  excluded.insert(Globals::camera.Addr);excluded.insert(Globals::localPlayer.GetModelRef().Addr);
  for(const auto& p:PlayerCache::players)excluded.insert(p.characterAddr);
  pending.push_back(Globals::dataModel.FindChildByClass("Workspace"));
 }
 const auto deadline=now+std::chrono::milliseconds(2);int budget=160;
 while(!pending.empty()&&budget-->0&&std::chrono::steady_clock::now()<deadline){
  auto node=pending.back();pending.pop_back();if(!node.Addr||excluded.count(node.Addr)||!visited.insert(node.Addr).second)continue;
  const auto cls=node.GetClass();
  if(cls=="Camera"||cls=="Tool"||cls=="Accessory"||cls=="Terrain")continue;
  auto children=node.GetChildList();bool humanoid=false;
  if(cls=="Model")for(auto child:children)if(child.GetClass()=="Humanoid"){humanoid=true;break;}
  if(humanoid)continue;
  if(cls=="Part"||cls=="MeshPart"||cls=="UnionOperation"||cls=="WedgePart"||cls=="CornerWedgePart"||cls=="TrussPart"){
   const float alpha=memory->Read<float>(node.Addr+Offsets::BasePart::Transparency);
   if(std::isfinite(alpha)&&alpha>=0&&alpha<.999f)staging.push_back(node.Addr);
  }
  if(cls=="Workspace"||cls=="Model"||cls=="Folder"||cls=="Part"||cls=="MeshPart"||cls=="UnionOperation")
   pending.insert(pending.end(),children.begin(),children.end());
  if(visited.size()>=60000){pending.clear();break;}
 }
 if(pending.empty()){parts.swap(staging);scanned=parts.size();next=std::chrono::steady_clock::now()+std::chrono::seconds(2);}
}
inline void Menu(){
 ImGui::SetCursorPos(ImVec2(12,51));ImGui::PushTextWrapPos(680);
 ImGui::TextUnformatted("World chams is temporarily unavailable in this recovery build. Its shared-object tracking caused a native-renderer regression and has been removed while character and weapon rendering are restored.");ImGui::PopTextWrapPos();
}
}
